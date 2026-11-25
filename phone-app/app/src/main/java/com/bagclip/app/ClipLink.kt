package com.bagclip.app

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.SharedPreferences
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import java.util.UUID

/**
 * The Bluetooth LE connection to the clip. One shared instance for the whole app: the
 * media listener feeds it songs, the screen shows its status.
 *
 * Android allows one GATT operation at a time, so every read/write goes through a small
 * queue that moves on when the previous one completes. Everything runs on the main thread.
 */
@SuppressLint("MissingPermission")   // every call is guarded by hasPermission() / try-catch
object ClipLink {
    private const val TAG = "ClipLink"
    private fun uuid(n: Int) = UUID.fromString("b4c1a00$n-6f1a-4a8e-9c3e-2f5d7a0e1c00")
    val SERVICE: UUID = uuid(0)
    private val SETTINGS = uuid(2)
    private val NOW = uuid(5)
    private val ART = uuid(6)
    private val BUTTONS = uuid(7)
    private val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    private lateinit var app: Context
    private lateinit var prefs: SharedPreferences
    private val main = Handler(Looper.getMainLooper())
    private var gatt: BluetoothGatt? = null
    private var discovered = false            // services found
    private var ready = false                 // services found, bonded, listening for taps
    private val ops = ArrayDeque<() -> Boolean>()
    private var busy = false
    private var nowPending: String? = null    // latest song text not yet written
    private var artPending: ByteArray? = null
    private var lastNow: String? = null       // what the clip has now (re-sent after reconnecting)
    private var lastArt: ByteArray? = null

    /** Taps on the clip: "toggle", "play", "pause", "next", "prev", "refresh". */
    var onButton: ((String) -> Unit)? = null
    /** Called whenever [state] or [clipSettings] changes, for the screen. */
    var onChange: (() -> Unit)? = null
    var state = "Not set up"
        private set
    var clipSettings: String? = null           // "brightness=200;dim=8;sleep=5;scroll=3;art=1"
        private set

    fun init(context: Context) {
        if (::app.isInitialized) return
        app = context.applicationContext
        prefs = app.getSharedPreferences("clip", Context.MODE_PRIVATE)
        val filter = IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED).apply {
            addAction(BluetoothAdapter.ACTION_STATE_CHANGED)
        }
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) = onBroadcast(i)
        }
        if (Build.VERSION.SDK_INT >= 33) app.registerReceiver(receiver, filter, Context.RECEIVER_EXPORTED)
        else app.registerReceiver(receiver, filter)
        connect()
    }

    val address: String? get() = prefs.getString("address", null)

    fun hasPermission(): Boolean = Permissions.bluetoothGranted(app)

    private fun adapter(): BluetoothAdapter? =
        (app.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter

    private fun setState(s: String) {
        if (s == state) return
        state = s
        Log.i(TAG, s)
        onChange?.invoke()
    }

    /** Remember a clip found by the scan and pair with it (the phone asks for the PIN). */
    fun use(device: BluetoothDevice) {
        prefs.edit().putString("address", device.address).apply()
        close()
        try {
            if (device.bondState == BluetoothDevice.BOND_NONE) device.createBond()
        } catch (e: SecurityException) { Log.w(TAG, "createBond", e) }
        connect()
    }

    fun forget() {
        val addr = address
        prefs.edit().remove("address").apply()
        close()
        setState("Not set up")
        // Removing the pairing needs a hidden API, so the screen asks the user to do it
        // in Bluetooth settings if they want to pair a different clip.
        Log.i(TAG, "forgot $addr")
    }

    /** Connects (and keeps reconnecting in the background) to the saved clip. */
    fun connect() {
        if (gatt != null) return
        val addr = address ?: return setState("Not set up")
        if (!hasPermission()) return setState("Needs Bluetooth permission")
        val ad = adapter()
        if (ad == null || !ad.isEnabled) return setState("Bluetooth is off")
        setState("Looking for the clip…")
        try {
            val dev = ad.getRemoteDevice(addr)
            // autoConnect = true: Android waits in the background and connects whenever the
            // clip is in range and awake, using little power on the phone.
            gatt = dev.connectGatt(app, true, callback, BluetoothDevice.TRANSPORT_LE)
        } catch (e: Exception) {
            Log.w(TAG, "connectGatt", e)
            setState("Can't connect: ${e.message}")
        }
    }

    private fun close() {
        discovered = false
        ready = false
        ops.clear()
        busy = false
        try { gatt?.close() } catch (_: SecurityException) {}
        gatt = null
    }

    private fun reconnectSoon() {
        close()
        main.postDelayed({ connect() }, 2000)
    }

    private fun onBroadcast(i: Intent) {
        when (i.action) {
            BluetoothAdapter.ACTION_STATE_CHANGED -> {
                when (i.getIntExtra(BluetoothAdapter.EXTRA_STATE, -1)) {
                    BluetoothAdapter.STATE_ON -> connect()
                    BluetoothAdapter.STATE_OFF -> { close(); setState("Bluetooth is off") }
                }
            }
            BluetoothDevice.ACTION_BOND_STATE_CHANGED -> {
                val dev: BluetoothDevice? = if (Build.VERSION.SDK_INT >= 33)
                    i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
                else @Suppress("DEPRECATION") i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
                if (dev?.address != address) return
                when (i.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, -1)) {
                    BluetoothDevice.BOND_BONDING -> setState("Pairing: type the PIN shown on the clip")
                    BluetoothDevice.BOND_BONDED -> if (discovered && !ready) startSession()
                    BluetoothDevice.BOND_NONE -> if (!ready) setState("Pairing failed or cancelled. Tap Find my clip to try again")
                }
            }
        }
    }

    // ---------- sending ----------

    /** Newline-separated key=value lines; see PROJECT.md "Bluetooth protocol". */
    fun sendNowPlaying(text: String) {
        lastNow = text
        if (!ready) return
        val queued = nowPending != null
        nowPending = text
        if (!queued) enqueue { nowPending?.let { nowPending = null; write(NOW, it.toByteArray()) } ?: false }
    }

    /** 128-byte thumbnail from [ArtDither], or an empty array for "no art". */
    fun sendArt(bytes: ByteArray) {
        lastArt = bytes
        if (!ready) return
        val queued = artPending != null
        artPending = bytes
        if (!queued) enqueue { artPending?.let { artPending = null; write(ART, it) } ?: false }
    }

    fun saveSettings(text: String) {
        if (!ready) return
        enqueue { write(SETTINGS, text.toByteArray()) }
        enqueue { read(SETTINGS) }
    }

    // ---------- GATT plumbing ----------

    private fun enqueue(op: () -> Boolean) {
        ops.addLast(op)
        drive()
    }

    private val watchdog = Runnable { Log.w(TAG, "operation timed out"); busy = false; drive() }

    private fun drive() {
        while (!busy && ops.isNotEmpty()) {
            val started = try { ops.removeFirst().invoke() } catch (e: SecurityException) { false }
            if (started) {
                busy = true
                main.postDelayed(watchdog, 10_000)
            }
        }
    }

    private fun opDone() {
        main.removeCallbacks(watchdog)
        busy = false
        drive()
    }

    private fun char(id: UUID): BluetoothGattCharacteristic? = gatt?.getService(SERVICE)?.getCharacteristic(id)

    @Suppress("DEPRECATION")
    private fun write(id: UUID, data: ByteArray): Boolean {
        val g = gatt ?: return false
        val c = char(id) ?: return false
        val type = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT   // with response: ordered, long writes work
        return if (Build.VERSION.SDK_INT >= 33) {
            g.writeCharacteristic(c, data, type) == BluetoothStatusCodes.SUCCESS
        } else {
            c.writeType = type
            c.value = data
            g.writeCharacteristic(c)
        }
    }

    private fun read(id: UUID): Boolean {
        val g = gatt ?: return false
        val c = char(id) ?: return false
        return g.readCharacteristic(c)
    }

    @Suppress("DEPRECATION")
    private fun enableTapNotifications(): Boolean {
        val g = gatt ?: return false
        val c = char(BUTTONS) ?: return false
        g.setCharacteristicNotification(c, true)
        val d = c.getDescriptor(CCCD) ?: return false
        val v = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
        return if (Build.VERSION.SDK_INT >= 33) {
            g.writeDescriptor(d, v) == BluetoothStatusCodes.SUCCESS
        } else {
            d.value = v
            g.writeDescriptor(d)
        }
    }

    /** After connecting and pairing: listen for taps, read settings, send the current song. */
    private fun startSession() {
        val g = gatt ?: return
        if (g.device.bondState != BluetoothDevice.BOND_BONDED) {
            setState("Pairing: type the PIN shown on the clip")
            g.device.createBond()
            return   // continues from the BOND_BONDED broadcast
        }
        enqueue { enableTapNotifications() }
        enqueue { read(SETTINGS) }
        enqueue {
            ready = true
            setState("Connected")
            g.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_LOW_POWER)
            lastNow?.let { sendNowPlaying(it) }
            lastArt?.let { sendArt(it) }
            onButton?.invoke("refresh")   // the listener sends a fresh update
            false
        }
    }

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            main.post {
                if (g != gatt) return@post
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    setState("Connecting…")
                    busy = true
                    main.postDelayed(watchdog, 10_000)
                    if (!g.requestMtu(247)) { busy = false; g.discoverServices() }
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    setState(if (address == null) "Not set up" else "Clip asleep or out of range")
                    reconnectSoon()
                }
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            main.post {
                main.removeCallbacks(watchdog)
                busy = false
                if (g == gatt) g.discoverServices()
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            main.post {
                if (g != gatt) return@post
                if (g.getService(SERVICE) == null) {
                    setState("That device isn't a Bag Clip")
                    return@post
                }
                discovered = true
                startSession()
            }
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            main.post {
                if (status != BluetoothGatt.GATT_SUCCESS) Log.w(TAG, "write ${c.uuid} failed: $status")
                opDone()
            }
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) {
            main.post { opDone() }
        }

        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray, status: Int) {
            main.post { gotRead(c.uuid, value, status) }
        }

        @Deprecated("Android 12 and older")
        override fun onCharacteristicRead(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) {
            if (Build.VERSION.SDK_INT >= 33) return
            @Suppress("DEPRECATION") val v = c.value ?: ByteArray(0)
            main.post { gotRead(c.uuid, v, status) }
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) {
            main.post { gotNotify(c.uuid, value) }
        }

        @Deprecated("Android 12 and older")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            if (Build.VERSION.SDK_INT >= 33) return
            @Suppress("DEPRECATION") val v = c.value ?: ByteArray(0)
            main.post { gotNotify(c.uuid, v) }
        }
    }

    private fun gotRead(id: UUID, value: ByteArray, status: Int) {
        if (status == BluetoothGatt.GATT_SUCCESS && id == SETTINGS) {
            clipSettings = String(value)
            onChange?.invoke()
        }
        opDone()
    }

    private fun gotNotify(id: UUID, value: ByteArray) {
        if (id == BUTTONS) onButton?.invoke(String(value).trim())
    }
}

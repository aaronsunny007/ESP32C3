package com.bagclip.app

import android.annotation.SuppressLint
import android.app.Activity
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.provider.Settings
import android.text.InputType
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import android.view.Gravity
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.TextView

/**
 * One screen: three setup steps, the connection status, and the clip's settings.
 * Built in code to keep the project small (no layout files or support libraries).
 */
@SuppressLint("MissingPermission", "SetTextI18n")
class MainActivity : Activity() {
    private val handler = Handler(Looper.getMainLooper())
    private lateinit var status: TextView
    private lateinit var song: TextView
    private lateinit var stepBt: Button
    private lateinit var stepMedia: Button
    private lateinit var stepFind: Button
    private lateinit var brightness: SeekBar
    private lateinit var scroll: SeekBar
    private lateinit var dim: EditText
    private lateinit var sleep: EditText
    private lateinit var art: CheckBox
    private var scanning = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ClipLink.init(applicationContext)
        val pad = dp(16)
        val col = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, pad, pad, pad)
        }
        setContentView(ScrollView(this).apply { addView(col) })

        // Panda header
        col.addView(LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            background = rounded(INK, 20)
            setPadding(dp(14), dp(10), dp(14), dp(10))
            addView(ImageView(context).apply {
                setImageResource(R.drawable.ic_clip)
                layoutParams = LinearLayout.LayoutParams(dp(48), dp(48))
            })
            addView(TextView(context).apply {
                text = "Bag Clip"
                textSize = 24f
                setTextColor(FUR)
                setTypeface(typeface, Typeface.BOLD)
                setPadding(dp(12), 0, 0, 0)
            })
        })

        status = text(col, "", 18f, bold = true)
        song = text(col, "", 15f)

        heading(col, "Set up")
        stepBt = button(col, "1. Allow Bluetooth") { requestPermissions(Permissions.bluetooth, 1) }
        stepMedia = button(col, "2. Allow reading what's playing") {
            startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS))
        }
        small(col, "Turn on Bag Clip in the list. Android calls this \"notification access\"; the app only " +
            "uses it to see the song and press play/pause/skip, and never reads your notifications. " +
            "If the switch is greyed out (Android 13+, app installed from a file): open App info for " +
            "Bag Clip, tap ⋮ (top right) → Allow restricted settings, then try again.")
        stepFind = button(col, "3. Find my clip") { scan() }
        small(col, "Wake the clip with a tap first. Your phone then asks for a PIN: type the 6 digits shown on the clip.")
        button(col, "Let it run in the background") {
            startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))
        }
        small(col, "Optional. Some phones (Xiaomi, Oppo, Vivo, Samsung) stop background apps; set Bag Clip " +
            "to \"Unrestricted\" / \"No restrictions\" so the clip keeps updating with the screen off.")

        heading(col, "Clip settings")
        small(col, "Brightness")
        brightness = seek(col, 255)
        small(col, "Title scroll speed")
        scroll = seek(col, 4)
        dim = number(col, "Dim the idle screen after (seconds, 0 = never)")
        sleep = number(col, "Sleep when nothing plays for (minutes, 0 = never)")
        art = CheckBox(this).apply { text = "Show album art"; col.addView(this) }
        button(col, "Save to clip") { saveSettings() }

        heading(col, "Other")
        button(col, "Forget this clip") {
            ClipLink.forget()
            refresh()
            status.text = "Forgotten. To pair a different clip, also remove BagClip under Settings → Bluetooth."
        }
        button(col, "Project page") {
            startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("https://github.com/aaronsunny007/esp32c3")))
        }
    }

    override fun onResume() {
        super.onResume()
        ClipLink.onChange = { handler.post { refresh() } }
        ClipLink.connect()
        MediaListener.rebindIfNeeded(this)
        refresh()
    }

    override fun onPause() {
        super.onPause()
        ClipLink.onChange = null
        stopScan()
    }

    override fun onRequestPermissionsResult(code: Int, perms: Array<out String>, results: IntArray) {
        ClipLink.connect()
        refresh()
    }

    private fun refresh() {
        val btOk = Permissions.bluetoothGranted(this)
        val mediaOk = Permissions.mediaAccessGranted(this)
        stepBt.text = (if (btOk) "✓ " else "") + "1. Allow Bluetooth"
        stepMedia.text = (if (mediaOk) "✓ " else "") + "2. Allow reading what's playing"
        stepFind.text = when {
            scanning -> "Searching… (tap to stop)"
            ClipLink.address != null -> "✓ 3. Find my clip (found)"
            else -> "3. Find my clip"
        }
        status.text = "Clip: ${ClipLink.state}"
        song.text = if (mediaOk) Status.nowPlaying else "Step 2 isn't done yet, so the clip can't see the song."
        ClipLink.clipSettings?.let { showSettings(it) }
    }

    // ---------- finding the clip ----------

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(type: Int, result: ScanResult) {
            stopScan()
            ClipLink.use(result.device)
            refresh()
        }
        override fun onScanFailed(errorCode: Int) {
            stopScan()
            status.text = "Search failed ($errorCode). Turn Bluetooth off and on, then try again."
        }
    }

    private fun scan() {
        if (scanning) { stopScan(); refresh(); return }
        if (!Permissions.bluetoothGranted(this)) { requestPermissions(Permissions.bluetooth, 1); return }
        val adapter = (getSystemService(BLUETOOTH_SERVICE) as BluetoothManager).adapter
        if (adapter == null || !adapter.isEnabled) { status.text = "Turn Bluetooth on first."; return }
        // Android 8-11 only finds Bluetooth devices while Location is switched on.
        if (Build.VERSION.SDK_INT < 31) {
            val lm = getSystemService(LOCATION_SERVICE) as android.location.LocationManager
            val on = if (Build.VERSION.SDK_INT >= 28) lm.isLocationEnabled
                     else lm.isProviderEnabled(android.location.LocationManager.GPS_PROVIDER) ||
                          lm.isProviderEnabled(android.location.LocationManager.NETWORK_PROVIDER)
            if (!on) { status.text = "Switch on Location (quick settings) so the phone can find the clip."; return }
        }
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(ClipLink.SERVICE)).build()
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        try {
            adapter.bluetoothLeScanner.startScan(listOf(filter), settings, scanCallback)
            scanning = true
            handler.postDelayed(scanTimeout, 20_000)
        } catch (e: Exception) {
            status.text = "Couldn't search: ${e.message}"
        }
        refresh()
    }

    private val scanTimeout = Runnable {
        if (scanning) {
            stopScan()
            refresh()
            status.text = "No clip found. Tap the clip to wake it, keep it close, and try again."
        }
    }

    private fun stopScan() {
        if (!scanning) return
        scanning = false
        handler.removeCallbacks(scanTimeout)
        try {
            (getSystemService(BLUETOOTH_SERVICE) as BluetoothManager).adapter?.bluetoothLeScanner?.stopScan(scanCallback)
        } catch (_: Exception) {}
    }

    // ---------- settings ----------

    private var shownSettings: String? = null

    private fun showSettings(s: String) {
        if (s == shownSettings) return
        shownSettings = s
        val kv = s.split(';').mapNotNull { p -> p.split('=').takeIf { it.size == 2 }?.let { it[0] to it[1] } }.toMap()
        kv["brightness"]?.toIntOrNull()?.let { brightness.progress = it }
        kv["scroll"]?.toIntOrNull()?.let { scroll.progress = it - 1 }
        kv["dim"]?.let { dim.setText(it) }
        kv["sleep"]?.let { sleep.setText(it) }
        kv["art"]?.let { art.isChecked = it == "1" }
    }

    private fun saveSettings() {
        val d = dim.text.toString().toIntOrNull()?.coerceIn(0, 3600) ?: 8
        val s = sleep.text.toString().toIntOrNull()?.coerceIn(0, 240) ?: 5
        val text = "brightness=${brightness.progress.coerceAtLeast(1)};dim=$d;sleep=$s;" +
            "scroll=${scroll.progress + 1};art=${if (art.isChecked) 1 else 0}"
        if (ClipLink.state != "Connected") { status.text = "Connect to the clip first (tap it to wake it)."; return }
        shownSettings = null
        ClipLink.saveSettings(text)
    }

    // ---------- tiny view helpers ----------

    private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()

    private fun rounded(color: Int, radiusDp: Int) =
        GradientDrawable().apply { setColor(color); cornerRadius = dp(radiusDp).toFloat() }

    private companion object {
        val INK = Color.parseColor("#1A1A1A")
        val FUR = Color.parseColor("#FAFAF6")
        val BAMBOO_DARK = Color.parseColor("#3B7142")
    }

    private fun text(parent: LinearLayout, s: String, size: Float, bold: Boolean = false) =
        TextView(this).apply {
            text = s
            textSize = size
            if (bold) setTypeface(typeface, Typeface.BOLD)
            setPadding(0, dp(4), 0, dp(4))
            parent.addView(this)
        }

    private fun heading(parent: LinearLayout, s: String) =
        text(parent, "\uD83C\uDF8B $s", 20f, bold = true).apply {
            setTextColor(BAMBOO_DARK)
            setPadding(0, dp(20), 0, dp(4))
        }

    private fun small(parent: LinearLayout, s: String) = text(parent, s, 13f).apply { alpha = 0.75f }

    private fun button(parent: LinearLayout, s: String, onClick: () -> Unit) =
        Button(this).apply {
            text = s
            isAllCaps = false
            setTextColor(FUR)
            background = rounded(INK, 24)
            stateListAnimator = null
            setOnClickListener { onClick() }
            parent.addView(this)
        }

    private fun seek(parent: LinearLayout, max: Int) =
        SeekBar(this).apply {
            this.max = max
            parent.addView(this)
        }

    private fun number(parent: LinearLayout, hint: String): EditText {
        small(parent, hint)
        return EditText(this).apply {
            inputType = InputType.TYPE_CLASS_NUMBER
            parent.addView(this)
        }
    }
}

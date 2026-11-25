package com.bagclip.app

import android.content.ComponentName
import android.graphics.Bitmap
import android.media.AudioManager
import android.media.MediaMetadata
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import android.os.BatteryManager
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.service.notification.NotificationListenerService
import android.util.Log
import android.view.KeyEvent
import java.text.Normalizer

/**
 * Watches what's playing on the phone (Spotify or any app with a media session) and sends
 * it to the clip; turns taps on the clip into play/pause/skip.
 *
 * Android only lets apps with "notification access" see other apps' media sessions, which
 * is why this is a NotificationListenerService. It ignores the notifications themselves.
 * Android keeps this service running in the background while access is granted.
 */
class MediaListener : NotificationListenerService() {
    private val handler = Handler(Looper.getMainLooper())
    private lateinit var sessions: MediaSessionManager
    private var controller: MediaController? = null
    private var connected = false
    private var lastCore = ""          // everything except the position, to skip duplicates
    private var lastArtKey = ""

    private val sessionsChanged = MediaSessionManager.OnActiveSessionsChangedListener { list ->
        pick(list ?: emptyList())
    }

    private val controllerCallback = object : MediaController.Callback() {
        override fun onMetadataChanged(metadata: MediaMetadata?) = push(force = true)
        override fun onPlaybackStateChanged(state: PlaybackState?) = push(force = true)
        override fun onQueueChanged(queue: MutableList<android.media.session.MediaSession.QueueItem>?) = push(force = false)
        override fun onSessionDestroyed() = refreshSessions()
    }

    // While playing, re-send the position every 30 s so the clip's progress bar stays exact
    // (it counts along by itself in between).
    private val tick = object : Runnable {
        override fun run() {
            if (isPlaying()) push(force = true)
            handler.postDelayed(this, 30_000)
        }
    }

    override fun onListenerConnected() {
        connected = true
        ClipLink.init(applicationContext)
        ClipLink.onButton = ::onButton
        sessions = getSystemService(MediaSessionManager::class.java)
        try {
            sessions.addOnActiveSessionsChangedListener(sessionsChanged, me(), handler)
        } catch (e: SecurityException) {
            Log.w(TAG, "no media access", e)
        }
        refreshSessions()
        ClipLink.connect()
        handler.postDelayed(tick, 30_000)
    }

    override fun onListenerDisconnected() {
        connected = false
        handler.removeCallbacks(tick)
        try { sessions.removeOnActiveSessionsChangedListener(sessionsChanged) } catch (_: Exception) {}
        controller?.unregisterCallback(controllerCallback)
        controller = null
        ClipLink.onButton = null
    }

    private fun me() = ComponentName(this, MediaListener::class.java)

    private fun refreshSessions() {
        val list = try { sessions.getActiveSessions(me()) } catch (e: SecurityException) { emptyList() }
        pick(list)
    }

    /** Follows the app that is playing; if none is, sticks with the current one. */
    private fun pick(list: List<MediaController>) {
        val playing = list.firstOrNull { it.playbackState?.state == PlaybackState.STATE_PLAYING }
        val keep = list.firstOrNull { it.sessionToken == controller?.sessionToken }
        val next = playing ?: keep ?: list.firstOrNull()
        if (next?.sessionToken != controller?.sessionToken) {
            controller?.unregisterCallback(controllerCallback)
            controller = next
            next?.registerCallback(controllerCallback, handler)
        }
        push(force = true)
    }

    private fun isPlaying() = controller?.playbackState?.state == PlaybackState.STATE_PLAYING

    private fun phoneBattery(): Int =
        (getSystemService(BATTERY_SERVICE) as BatteryManager).getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY)

    private fun push(force: Boolean) {
        val c = controller
        val meta = c?.metadata
        val title = meta?.getString(MediaMetadata.METADATA_KEY_TITLE)
            ?: meta?.description?.title?.toString()
        if (c == null || meta == null || title.isNullOrBlank()) {
            val core = "clear=1\npbat=${phoneBattery()}"
            if (force || core != lastCore) { lastCore = core; ClipLink.sendNowPlaying(core) }
            if (lastArtKey != "") { lastArtKey = ""; ClipLink.sendArt(ByteArray(0)) }
            Status.nowPlaying = "Nothing playing"
            return
        }
        val artist = meta.getString(MediaMetadata.METADATA_KEY_ARTIST)
            ?: meta.getString(MediaMetadata.METADATA_KEY_ALBUM_ARTIST)
            ?: meta.description?.subtitle?.toString() ?: ""
        val state = c.playbackState
        val playing = state?.state == PlaybackState.STATE_PLAYING
        val dur = meta.getLong(MediaMetadata.METADATA_KEY_DURATION).coerceAtLeast(0)
        val next = nextInQueue(c)

        val core = buildString {
            append("title=").append(clean(title)).append('\n')
            append("artist=").append(clean(artist)).append('\n')
            append("next=").append(clean(next)).append('\n')
            append("dur=").append(dur).append('\n')
            append("play=").append(if (playing) 1 else 0).append('\n')
            append("app=").append(clean(appName(c.packageName))).append('\n')
            append("pbat=").append(phoneBattery())
        }
        if (force || core != lastCore) {
            lastCore = core
            ClipLink.sendNowPlaying("$core\npos=${position(state)}")
        }
        Status.nowPlaying = "${if (playing) "▶" else "⏸"} $title — $artist"

        // Album art: converted once per song (or when the app supplies a better picture).
        val bmp: Bitmap? = meta.getBitmap(MediaMetadata.METADATA_KEY_ALBUM_ART)
            ?: meta.getBitmap(MediaMetadata.METADATA_KEY_ART)
            ?: meta.getBitmap(MediaMetadata.METADATA_KEY_DISPLAY_ICON)
        val artKey = "$title|$artist|${bmp?.generationId}|${bmp?.width}"
        if (artKey != lastArtKey) {
            lastArtKey = artKey
            val bytes = try { bmp?.let { ArtDither.convert(it) } } catch (e: Exception) { null }
            ClipLink.sendArt(bytes ?: ByteArray(0))
        }
    }

    private fun position(s: PlaybackState?): Long {
        if (s == null || s.position < 0) return 0
        if (s.state != PlaybackState.STATE_PLAYING) return s.position
        val since = SystemClock.elapsedRealtime() - s.lastPositionUpdateTime
        return s.position + (since * s.playbackSpeed).toLong()
    }

    /** Next song, if the playing app shares its queue (many do; some don't). */
    private fun nextInQueue(c: MediaController): String {
        val queue = c.queue ?: return ""
        val active = c.playbackState?.activeQueueItemId ?: return ""
        val i = queue.indexOfFirst { it.queueId == active }
        if (i < 0 || i + 1 >= queue.size) return ""
        return queue[i + 1].description.title?.toString() ?: ""
    }

    private fun appName(pkg: String): String = KNOWN_APPS[pkg] ?: try {
        packageManager.getApplicationLabel(packageManager.getApplicationInfo(pkg, 0)).toString()
    } catch (e: Exception) {
        pkg.substringAfterLast('.')
    }

    /** Taps on the clip. */
    private fun onButton(cmd: String) {
        val tc = controller?.transportControls
        when (cmd) {
            "toggle" -> if (tc == null) mediaKey(KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE)
                        else if (isPlaying()) tc.pause() else tc.play()
            "play" -> tc?.play() ?: mediaKey(KeyEvent.KEYCODE_MEDIA_PLAY)
            "pause" -> tc?.pause() ?: mediaKey(KeyEvent.KEYCODE_MEDIA_PAUSE)
            "next" -> tc?.skipToNext() ?: mediaKey(KeyEvent.KEYCODE_MEDIA_NEXT)
            "prev" -> tc?.skipToPrevious() ?: mediaKey(KeyEvent.KEYCODE_MEDIA_PREVIOUS)
            "refresh" -> push(force = true)
        }
    }

    /** No media session yet (e.g. Spotify was closed): a media key wakes the last player. */
    private fun mediaKey(code: Int) {
        val am = getSystemService(AUDIO_SERVICE) as AudioManager
        am.dispatchMediaKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, code))
        am.dispatchMediaKeyEvent(KeyEvent(KeyEvent.ACTION_UP, code))
    }

    companion object {
        private const val TAG = "MediaListener"
        private val KNOWN_APPS = mapOf(
            "com.spotify.music" to "Spotify",
            "com.google.android.apps.youtube.music" to "YT Music",
            "com.google.android.youtube" to "YouTube",
            "com.jio.media.jiobeats" to "JioSaavn",
            "com.bsbportal.music" to "Wynk",
            "com.amazon.mp3" to "Amazon Music",
            "com.apple.android.music" to "Apple Music",
            "deezer.android.app" to "Deezer",
            "com.soundcloud.android" to "SoundCloud",
        )

        /** The clip's font only has plain ASCII: "Beyoncé" becomes "Beyonce", other scripts "?". */
        fun clean(s: String): String {
            val noAccents = Normalizer.normalize(s, Normalizer.Form.NFD).replace(Regex("\\p{M}+"), "")
            return buildString {
                for (ch in noAccents) append(if (ch == '\n' || ch == '\r') ' ' else if (ch.code in 32..126) ch else '?')
            }.take(90)
        }
    }
}

/** What the screen shows about the current song. */
object Status {
    var nowPlaying: String = "Nothing playing"
        set(v) { if (field != v) { field = v; ClipLink.onChange?.invoke() } }
}

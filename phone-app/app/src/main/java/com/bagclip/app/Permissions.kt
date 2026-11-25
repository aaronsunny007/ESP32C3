package com.bagclip.app

import android.Manifest
import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.provider.Settings

object Permissions {
    /** Runtime permissions needed to find and talk to the clip. */
    val bluetooth: Array<String> =
        if (Build.VERSION.SDK_INT >= 31) arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)

    fun bluetoothGranted(c: Context) =
        bluetooth.all { c.checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }

    /** "Notification access", which is what lets the app see and control other apps' media. */
    fun mediaAccessGranted(c: Context): Boolean {
        val enabled = Settings.Secure.getString(c.contentResolver, "enabled_notification_listeners") ?: return false
        val me = ComponentName(c, MediaListener::class.java)
        return enabled.split(':').any { ComponentName.unflattenFromString(it) == me }
    }
}

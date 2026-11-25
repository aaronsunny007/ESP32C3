package com.bagclip.app

import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect

/**
 * Turns album art into the clip's 32x32 black-and-white thumbnail: centre-crop, shrink,
 * stretch the contrast so dark covers still show detail, then Floyd-Steinberg dither.
 * Output: 128 bytes, rows top to bottom, 4 bytes per row, most significant bit = leftmost
 * pixel, 1 = lit (the Adafruit drawBitmap format the firmware uses).
 */
object ArtDither {
    const val SIZE = 32

    fun convert(src: Bitmap): ByteArray {
        val small = Bitmap.createBitmap(SIZE, SIZE, Bitmap.Config.ARGB_8888)
        val side = minOf(src.width, src.height)
        val crop = Rect((src.width - side) / 2, (src.height - side) / 2,
            (src.width + side) / 2, (src.height + side) / 2)
        // Drawing (rather than getPixels on the source) also works for hardware bitmaps.
        Canvas(small).drawBitmap(src, crop, Rect(0, 0, SIZE, SIZE), Paint(Paint.FILTER_BITMAP_FLAG))

        val px = IntArray(SIZE * SIZE)
        small.getPixels(px, 0, SIZE, 0, 0, SIZE, SIZE)
        small.recycle()

        val gray = IntArray(px.size) { i ->
            val c = px[i]
            val r = (c shr 16) and 0xFF
            val g = (c shr 8) and 0xFF
            val b = c and 0xFF
            (r * 77 + g * 150 + b * 29) shr 8
        }
        val lo = gray.min()
        val range = maxOf(1, gray.max() - lo)
        val err = IntArray(gray.size) { (gray[it] - lo) * 255 / range }

        val out = ByteArray(SIZE * SIZE / 8)
        for (y in 0 until SIZE) {
            for (x in 0 until SIZE) {
                val i = y * SIZE + x
                val v = err[i]
                val lit = if (v >= 128) 255 else 0
                if (lit != 0) {
                    val idx = y * (SIZE / 8) + x / 8
                    out[idx] = (out[idx].toInt() or (0x80 ushr (x and 7))).toByte()
                }
                val e = v - lit
                if (x + 1 < SIZE) err[i + 1] += e * 7 / 16
                if (y + 1 < SIZE) {
                    if (x > 0) err[i + SIZE - 1] += e * 3 / 16
                    err[i + SIZE] += e * 5 / 16
                    if (x + 1 < SIZE) err[i + SIZE + 1] += e / 16
                }
            }
        }
        return out
    }
}

package de.wiomoc.gifply

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.*
import android.os.Handler
import android.os.Looper
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import kotlin.math.min

class DeviceManager(val context: Context, val listener: Listener) {
    interface Listener {
        fun onConnect()

        fun onDisconnect()
    }

    private val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager

    private var connectedDevice: DeviceConnection? = null

    private var executor: ExecutorService? = null

    val VID = 0xCafe
    val PID = 0x0420
    private val PERMISSION_GRANTED_ACTION = "de.wiomoc.giffy.PermissionGranted"

    val connected: Boolean
        get() = connectedDevice != null

    private class DeviceConnection(val device: UsbDevice, val usbManager: UsbManager) {
        lateinit var deviceConnection: UsbDeviceConnection
        lateinit var interfac: UsbInterface
        lateinit var inEndpoint: UsbEndpoint
        lateinit var outEndpoint: UsbEndpoint

        fun open() {
            deviceConnection = usbManager.openDevice(device)
            interfac = device.getConfiguration(0).getInterface(0)
            outEndpoint = interfac.getEndpoint(0)
            inEndpoint = interfac.getEndpoint(1)
            deviceConnection.claimInterface(interfac, true)
        }

        fun readGif(num: Int): ByteArray {
            deviceConnection.controlTransfer(2 shl 5 or 1, if (num == 1) 1 else 2, 0, 0, byteArrayOf(), 0, 0)
            val buf = ByteArray(256)
            deviceConnection.bulkTransfer(inEndpoint, buf, buf.size, 0)
            val len = ByteBuffer.wrap(buf).order(ByteOrder.LITTLE_ENDIAN).int
            val gif = ByteArray(len)
            System.arraycopy(buf, 4, gif, 0, 252)
            var pos = 252
            while (len > pos) {
                val chunkSize = min(buf.size, len - pos)
                deviceConnection.bulkTransfer(inEndpoint, buf, chunkSize, 0)
                System.arraycopy(buf, 0, gif, pos, chunkSize)
                pos += buf.size
            }
            return gif
        }

        fun close() {
            deviceConnection.close()
        }

        fun writeGif(num: Int, bytes: ByteArray) {
            deviceConnection.controlTransfer(2 shl 5 or 1, if (num == 1) 3 else 4, 0, 0, byteArrayOf(), 0, 0)

            val first = ByteBuffer.allocate(256)
                .order(ByteOrder.LITTLE_ENDIAN)
                .putInt(bytes.size)
                .put(bytes, 0, 252)
                .array()

            deviceConnection.bulkTransfer(outEndpoint, first, first.size, 0)

            for (i in 252 until bytes.size step 256) {
                println("pos $i, ${min(256, bytes.size - i)}")
                deviceConnection.bulkTransfer(outEndpoint, bytes, i, min(256, bytes.size - i), 0)
            }
        }
    }

    fun start() {
        context.registerReceiver(object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent) {
                val device = (intent.extras?.get(UsbManager.EXTRA_DEVICE) ?: return) as UsbDevice

                when (intent.action) {
                    UsbManager.ACTION_USB_DEVICE_ATTACHED -> {
                        handleDevice(device)
                    }
                    UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                        if (connectedDevice?.device == device) {
                            connectedDevice?.close()
                            connectedDevice = null
                            listener.onDisconnect()
                        }
                    }
                    PERMISSION_GRANTED_ACTION -> {
                        if (intent.extras?.getBoolean(UsbManager.EXTRA_PERMISSION_GRANTED) == true) {
                            connectToDevice(device)
                        }
                    }
                }
            }

        }, IntentFilter().apply {
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
            addAction(PERMISSION_GRANTED_ACTION)
        })

        usbManager.deviceList.values.forEach(::handleDevice)
    }

    private fun handleDevice(device: UsbDevice) {
        if (device.vendorId != VID || device.productId != PID || connected) {
            return
        }
        if (usbManager.hasPermission(device)) {
            connectToDevice(device)
        } else {
            usbManager.requestPermission(
                device,
                PendingIntent.getBroadcast(context, 1, Intent(PERMISSION_GRANTED_ACTION), 0)
            )
        }
    }

    private fun connectToDevice(device: UsbDevice) {
        connectedDevice = DeviceConnection(device, usbManager)
        if (executor == null) executor = Executors.newSingleThreadExecutor()
        executor!!.execute {
            connectedDevice?.open()

            Handler(Looper.getMainLooper()).post {
                listener.onConnect()
            }
        }
    }

    fun readGif(num: Int, callback: (ByteArray?) -> Unit) {
        executor!!.execute {
            val result = this.connectedDevice?.readGif(num)
            Handler(Looper.getMainLooper()).post {
                callback(result)
            }
        }
    }

    fun writeGif(num: Int, bytes: ByteArray, callback: (Boolean) -> Unit) {
        executor!!.execute {
            this.connectedDevice?.writeGif(num, bytes)
            Handler(Looper.getMainLooper()).post {
                callback(true)
            }
        }
    }
}

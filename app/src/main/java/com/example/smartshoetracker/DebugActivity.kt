package com.example.smartshoetracker // UPDATE PACKAGE NAME

import android.Manifest
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import java.util.*

class DebugActivity : AppCompatActivity() {

    private lateinit var tvConsole: TextView
    private lateinit var tvStatus: TextView
    private var bluetoothGatt: BluetoothGatt? = null
    private val bluetoothAdapter: BluetoothAdapter? by lazy {
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothManager.adapter
    }

    private val SERVICE_UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    private val CHAR_TX_UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_debug)

        tvConsole = findViewById(R.id.tvConsole)
        tvStatus = findViewById(R.id.tvStatus)

        checkPermissionsAndStart()
    }

    private fun checkPermissionsAndStart() {
        val permissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        if (permissions.any { ActivityCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED }) {
            ActivityCompat.requestPermissions(this, permissions, 2)
        } else {
            startBleScan()
        }
    }

    private fun startBleScan() {
        try {
            bluetoothAdapter?.bluetoothLeScanner?.startScan(scanCallback)
        } catch (e: SecurityException) {
            tvStatus.text = "Permission missing."
        }
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            try {
                if (result.device.name == "SmartShoe_BLE") {
                    bluetoothAdapter?.bluetoothLeScanner?.stopScan(this)
                    tvStatus.text = "Found Shoe! Connecting..."
                    bluetoothGatt = result.device.connectGatt(this@DebugActivity, false, gattCallback)
                }
            } catch (e: SecurityException) {}
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            try {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    runOnUiThread { tvStatus.text = "Connected! Reading Logs..." }
                    gatt.discoverServices()
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    runOnUiThread { tvStatus.text = "Disconnected." }
                }
            } catch (e: SecurityException) {}
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            try {
                val service = gatt.getService(SERVICE_UUID)
                val txChar = service?.getCharacteristic(CHAR_TX_UUID)
                if (txChar != null) {
                    gatt.setCharacteristicNotification(txChar, true)
                    val descriptor = txChar.descriptors[0]
                    descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    gatt.writeDescriptor(descriptor)
                }
            } catch (e: SecurityException) {}
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            val message = characteristic.getStringValue(0)
            runOnUiThread {
                tvConsole.append(message + "\n")
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        try {
            bluetoothGatt?.close()
        } catch (e: SecurityException) {}
    }
}
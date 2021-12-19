package de.wiomoc.gifply

import android.app.Dialog
import android.net.Uri
import android.os.AsyncTask
import android.os.Bundle
import android.view.View
import android.widget.ImageView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.DialogFragment
import com.bumptech.glide.Glide
import java.io.File
import java.io.FileOutputStream
import java.net.URL

class UploadDialog(private val deviceManager: DeviceManager, private val image: Uri) : DialogFragment() {
    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        return activity?.let {
            val builder = AlertDialog.Builder(it)
            val view = View.inflate(context, R.layout.upload_dialog, null)
            deviceManager.readGif(1) {
                val imageView: ImageView = view.findViewById(R.id.gif_1)
                imageView.setOnClickListener {
                    uploadGif(1)
                }
                Glide.with(requireContext()).asGif().load(it).into(imageView)
            }
            deviceManager.readGif(2) {
                val imageView: ImageView = view.findViewById(R.id.gif_2)
                imageView.setOnClickListener {
                    uploadGif(2)
                }
                Glide.with(requireContext()).asGif().load(it).into(imageView)
            }
            builder.setView(view)
            builder.setNegativeButton(
                "Cancel"
            ) { dialog, _ -> dialog.cancel() }
            builder.create()
        } ?: throw IllegalStateException("Activity cannot be null")
    }

    fun uploadGif(num: Int) {
        val outputFile = File.createTempFile("out", ".gif")

        val task = object : AsyncTask<Void, Void, Boolean>() {
            override fun doInBackground(vararg params: Void?): Boolean {
                val inputFile = File.createTempFile("input", ".gif")

                val conn = URL(image.toString()).openConnection()
                conn.getInputStream().copyTo(FileOutputStream(inputFile))

                GifProcessor.process(inputFile.absolutePath, outputFile.absolutePath)
                val bytes = outputFile.readBytes()
                if (bytes.size > 512 * 1024) {
                    return false
                }

                deviceManager.writeGif(num, bytes) {
                    dialog?.dismiss()
                }
                return true
            }

            override fun onPostExecute(result: Boolean) {
                super.onPostExecute(result)
                if (!result) {
                    Toast.makeText(context, "Upload failed - GIF to large", Toast.LENGTH_SHORT).show()
                }
            }

        }
        task.execute()
    }

}

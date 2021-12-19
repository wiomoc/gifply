package de.wiomoc.gifply

import android.net.Uri
import android.os.AsyncTask
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.bumptech.glide.Glide
import kotlinx.android.synthetic.main.activity_main.*
import org.json.JSONObject
import java.io.InputStreamReader
import java.net.URL

class MainActivity : AppCompatActivity() {
    lateinit var deviceManager: DeviceManager
    override fun onCreate(savedInstanceState: Bundle?) {


        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        deviceManager = DeviceManager(this, object : DeviceManager.Listener {
            override fun onConnect() {
                println("OnConnect")
            }

            override fun onDisconnect() {
                println("OnDisconnect")
            }
        })

        deviceManager.start()
        val adapter = GiphyAdapter()
        var offset = 0
        var loading = true

        val gliphyListLoader = GiphyListLoader {
            adapter.images.addAll(it)
            adapter.notifyDataSetChanged()
        }
        gliphyListLoader.tryLoadNext()
        val layoutManager = GridLayoutManager(this, 2)
        gif_grid.layoutManager = layoutManager
        gif_grid.adapter = adapter
        gif_grid.addOnScrollListener(object : RecyclerView.OnScrollListener() {
            override fun onScrolled(recyclerView: RecyclerView, dx: Int, dy: Int) {
                if (layoutManager.findLastVisibleItemPosition() == adapter.images.size - 1) {
                    println("Refresh")
                    gliphyListLoader.tryLoadNext()
                }
            }
        })
    }

    data class GiphyImage(val smallGif: Uri, val originalGif: Uri)

    class GiphyListLoader(val callback: (List<GiphyImage>) -> Unit) {
        var offset = 0
        var loading = false

        inner class LoadGiphyListAsyncTask : AsyncTask<Void, Void, List<GiphyImage>>() {
            override fun doInBackground(vararg params: Void?): List<GiphyImage> {
                val conn =
                    URL("https://api.giphy.com/v1/gifs/trending?api_key=${BuildConfig.GIPHY_API_KEY}&offset=$offset").openConnection()
                offset += 50
                val json = InputStreamReader(conn.getInputStream()).readText()
                val array = JSONObject(json).getJSONArray("data")
                return (0 until array.length()).map {
                    val images = array.getJSONObject(it).getJSONObject("images")
                    GiphyImage(
                        Uri.parse(
                            images.getJSONObject("fixed_height").getString("url")
                        ),
                        Uri.parse(
                            images.getJSONObject("original").getString("url")
                        )
                    )
                }
            }

            override fun onPostExecute(result: List<GiphyImage>) {
                callback(result)
                loading = false
            }
        }

        fun tryLoadNext() {
            if (loading) return
            loading = true
            LoadGiphyListAsyncTask().execute()
        }
    }

    class GiphyViewHolder(val activity: MainActivity) :
        RecyclerView.ViewHolder(View.inflate(activity, R.layout.image_tile, null)) {

        lateinit var image: GiphyImage

        init {
            itemView.setOnClickListener {
                if (activity.deviceManager.connected) {
                    UploadDialog(activity.deviceManager, image.originalGif).show(activity.supportFragmentManager, "abc")
                } else {
                    Toast.makeText(itemView.context, "Not Connected", Toast.LENGTH_SHORT).show()
                }
            }
        }

        fun bind(image: GiphyImage) {
            this.image = image
            Glide.with(activity)
                .asGif()
                .load(image.smallGif)
                .centerCrop()
                .into(itemView.findViewById(R.id.image_view))
        }
    }

    inner class GiphyAdapter(var images: MutableList<GiphyImage> = mutableListOf()) :
        RecyclerView.Adapter<GiphyViewHolder>() {
        init {
            setHasStableIds(true)
        }

        override fun getItemId(position: Int): Long {
            return position.toLong()
        }


        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GiphyViewHolder {
            return GiphyViewHolder(this@MainActivity)
        }

        override fun onBindViewHolder(holder: GiphyViewHolder, position: Int) {
            holder.bind(images[position])
        }

        override fun getItemCount(): Int {
            return images.size
        }
    }
}


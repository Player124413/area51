package com.a51.android.ui

import android.text.format.Formatter
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.a51.android.databinding.ItemFileBinding

/**
 * Files inside one .dfs archive.
 */
class FileAdapter(
    private val onClick: (FileEntry) -> Unit
) : RecyclerView.Adapter<FileAdapter.Holder>() {

    data class FileEntry(val index: Int, val name: String, val size: Long, val offset: Long)

    private val items = ArrayList<FileEntry>()

    fun addAll(list: List<FileEntry>) {
        val start = items.size
        items.addAll(list)
        notifyItemRangeInserted(start, list.size)
    }

    fun clear() {
        items.clear()
        notifyDataSetChanged()
    }

    fun entryAt(position: Int): FileEntry? = items.getOrNull(position)

    override fun getItemCount(): Int = items.size

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
        val binding = ItemFileBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return Holder(binding)
    }

    override fun onBindViewHolder(holder: Holder, position: Int) = holder.bind(items[position])

    inner class Holder(private val binding: ItemFileBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(entry: FileEntry) {
            binding.tvName.text = entry.name
            binding.tvSize.text = Formatter.formatFileSize(binding.root.context, entry.size)
            binding.root.setOnClickListener { onClick(entry) }
        }
    }
}

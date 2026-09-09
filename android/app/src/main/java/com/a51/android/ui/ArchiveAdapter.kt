package com.a51.android.ui

import android.text.format.Formatter
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.a51.android.data.DataImporter
import com.a51.android.databinding.ItemArchiveBinding

/**
 * Check list of the files found in the picked folder.
 *
 * Data splits (.000, .001, ...) are not listed on their own - they follow the
 * archive they belong to, see [DataImporter.importEntries].
 */
class ArchiveAdapter : RecyclerView.Adapter<ArchiveAdapter.Holder>() {

    private val items = ArrayList<DataImporter.SafEntry>()
    private val checked = HashSet<String>()

    fun submit(list: List<DataImporter.SafEntry>) {
        items.clear()
        items.addAll(list)
        checked.clear()
        items.forEach { checked.add(it.name.lowercase()) }
        notifyDataSetChanged()
    }

    fun selectedEntries(): List<DataImporter.SafEntry> =
        items.filter { it.name.lowercase() in checked }

    fun totalSelectedBytes(): Long = selectedEntries().sumOf { it.size.coerceAtLeast(0L) }

    override fun getItemCount(): Int = items.size

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
        val binding = ItemArchiveBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return Holder(binding)
    }

    override fun onBindViewHolder(holder: Holder, position: Int) = holder.bind(items[position])

    inner class Holder(private val binding: ItemArchiveBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(entry: DataImporter.SafEntry) {
            val ctx = binding.root.context
            binding.tvName.text = entry.name
            binding.tvMeta.text = Formatter.formatFileSize(ctx, entry.size)
            binding.cbSelect.isChecked = entry.name.lowercase() in checked
            binding.cbSelect.setOnCheckedChangeListener { _, on ->
                if (on) checked.add(entry.name.lowercase())
                else checked.remove(entry.name.lowercase())
            }
            binding.root.setOnClickListener {
                binding.cbSelect.isChecked = !binding.cbSelect.isChecked
            }
        }
    }
}

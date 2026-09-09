package com.a51.android.ui

import android.text.format.Formatter
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.a51.android.R
import com.a51.android.data.GameProfile
import com.a51.android.databinding.ItemGameBinding

/**
 * One card per imported game data set.
 */
class GameAdapter(
    private val onPlay: (GameProfile) -> Unit,
    private val onFiles: (GameProfile) -> Unit,
    private val onDelete: (GameProfile) -> Unit
) : RecyclerView.Adapter<GameAdapter.Holder>() {

    private val items = ArrayList<GameProfile>()

    fun submit(list: List<GameProfile>) {
        items.clear()
        items.addAll(list)
        notifyDataSetChanged()
    }

    override fun getItemCount(): Int = items.size

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): Holder {
        val binding = ItemGameBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return Holder(binding)
    }

    override fun onBindViewHolder(holder: Holder, position: Int) {
        holder.bind(items[position])
    }

    inner class Holder(private val binding: ItemGameBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(profile: GameProfile) {
            val ctx = binding.root.context
            binding.tvName.text = profile.name
            binding.tvMeta.text = ctx.getString(
                R.string.import_found,
                profile.archiveCount,
                Formatter.formatFileSize(ctx, profile.sizeBytes)
            ) + "  ·  " + profile.fileCount + " files"

            binding.tvStatus.text = when {
                profile.archiveCount <= 0 -> "no archives"
                else -> "ready"
            }
            binding.tvStatus.setTextColor(
                ctx.getColor(
                    if (profile.archiveCount > 0) R.color.a51_green else R.color.a51_red
                )
            )

            binding.btnPlay.setOnClickListener { onPlay(profile) }
            binding.btnFiles.setOnClickListener { onFiles(profile) }
            binding.btnDelete.setOnClickListener { onDelete(profile) }
        }
    }
}

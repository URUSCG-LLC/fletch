// Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

package com.google.fletch.githubsample;

import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.google.fletch.immisamples.SlidingWindow;

import immi.AnyNode;
import immi.CommitNode;

public class RecyclerViewAdapter extends SlidingWindow<RecyclerViewAdapter.CommitViewHolder> {

  ImageLoader imageLoader;

  private static final int IMAGE_VIEW_DIMENSION_PX = 120;

  RecyclerViewAdapter(ImageLoader imageLoader) {
    this.imageLoader = imageLoader;
  }

  @Override
  public CommitViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
    View view =
        LayoutInflater.from(parent.getContext()).inflate(R.layout.cards_layout, parent, false);
    return new CommitViewHolder(view);
  }

  @Override
  public void onBindViewHolder(CommitViewHolder holder, AnyNode node) {
    holder.setLoading(node == null);
    if (node == null) return;
    CommitCardView commitView = holder.cardView;
    // TODO(zerny): Should this population be moved to the CommitViewCard?
    commitView.setCommitNode(node.as(CommitNode.class));
    holder.author.setText(commitView.getAuthor());
    holder.title.setText(commitView.getTitle());
    imageLoader.loadImageFromUrl(holder.avatar, commitView.getImageUrl(),
        IMAGE_VIEW_DIMENSION_PX, IMAGE_VIEW_DIMENSION_PX);
  }

  @Override
  public void onAttachedToRecyclerView(RecyclerView recyclerView) {
    super.onAttachedToRecyclerView(recyclerView);
  }

  public static class CommitViewHolder extends RecyclerView.ViewHolder {

    CommitCardView cardView;
    ProgressBar loadingView;
    View loadedView;
    TextView author;
    TextView title;
    ImageView avatar;

    public CommitViewHolder(View itemView) {
      super(itemView);
      cardView = (CommitCardView)itemView.findViewById(R.id.card_view);
      loadingView = (ProgressBar)itemView.findViewById(R.id.card_loading);
      loadedView = itemView.findViewById(R.id.card_loaded);
      author = (TextView)itemView.findViewById(R.id.author);
      title = (TextView)itemView.findViewById(R.id.title);
      avatar = (ImageView)itemView.findViewById(R.id.avatar);
      setLoading(true);
    }

    public void setLoading(boolean loading) {
      this.loading = loading;
      if (loading) {
        loadedView.setVisibility(View.GONE);
        loadingView.setVisibility(View.VISIBLE);
      } else {
        loadedView.setVisibility(View.VISIBLE);
        loadingView.setVisibility(View.GONE);
      }
    }

    private boolean loading;
  }
}

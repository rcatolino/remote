package com.rcatolino.remoteclient;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentPagerAdapter;
import android.support.v4.view.ViewPager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

public class ImageViewAdapter extends FragmentPagerAdapter
                              implements ViewPager.OnPageChangeListener {

  private static final String LOGTAG = "RemoteClient/ImageViewAdapter";
  private static final int ITEM_NB = 3;
  private final ViewPager pager;
  private ImageViewFragment[] fragments;

  private static class ImageViewFragment extends Fragment {

    private ImageView imageIV;
    private int backgroundSrc = 0;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle bundle) {

      View rootView = inflater.inflate(R.layout.image_fragment,
                                       container, false);
      imageIV = (ImageView) rootView.findViewById(R.id.image_view);
      if (backgroundSrc != 0) {
        imageIV.setImageResource(backgroundSrc);
      }
      return rootView;
    }

    public void setBackground(int id) {
      Log.d(LOGTAG, "FUCKIN ID YO " + id + ", FUCKIN RES YO " + imageIV);
      if (id == backgroundSrc) {
        return;
      }
      if (imageIV != null) {
        imageIV.setImageResource(id);
      }
      backgroundSrc = id;
    }
  }

  public ImageViewAdapter (FragmentManager fm, ViewPager vp) {
    super(fm);
    pager = vp;
    pager.setAdapter(this);
    pager.setOnPageChangeListener(this);
    fragments = new ImageViewFragment[3];
    fragments[1] = new ImageViewFragment();
    pager.setCurrentItem(1, false);
  }

  public void setBackground(int id) {
    if (fragments[1] == null) {
      fragments[1] = new ImageViewFragment();
    }

    fragments[1].setBackground(id);
  }

  @Override
  public Fragment getItem(int i) {
    Log.d(LOGTAG, "View pager ask for new item");
    if (fragments[i] == null) {
      fragments[i] = new ImageViewFragment();
    }
    return fragments[i];
  }

  @Override
  public int getCount() {
    return ITEM_NB;
  }

  @Override
  public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
    Log.d(LOGTAG, "Pager page scrolled");
  }

  @Override
  public void onPageSelected(int position) {
    Log.d(LOGTAG, "Pager page selected : " + position);
  }

  @Override
  public void onPageScrollStateChanged(int state) {
    Log.d(LOGTAG, "Pager page scroll state changed : " + state);
    if (state == ViewPager.SCROLL_STATE_IDLE) {
      setBackground(R.drawable.cover_unknown);
      Log.d(LOGTAG, "Changing page");
      pager.setCurrentItem(1, false);
    }
  }
}

package com.rcatolino.remoteclient;

import android.graphics.Bitmap;
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
  private OnPreviousNextListener pnListener;

  public interface OnPreviousNextListener {
    public abstract void onPrevious();
    public abstract void onNext();
  }

  public static class ImageViewFragment extends Fragment {

    private ImageView imageIV;
    private int backgroundSrc = 0;

    public ImageViewFragment() {
      super();
    }

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
      if (id == backgroundSrc) {
        return;
      }
      if (imageIV != null) {
        imageIV.setImageResource(id);
      }
      backgroundSrc = id;
    }

    public void setBackground(Bitmap bm) {
      imageIV.setImageBitmap(bm);
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
    pnListener = null;
  }

  public void setOnPreviousNextListener(OnPreviousNextListener listener) {
    pnListener = listener;
  }

  public void setBackground(Bitmap bm) {
    if (fragments[1] == null) {
      fragments[1] = new ImageViewFragment();
    }

    fragments[1].setBackground(bm);
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
  }

  @Override
  public void onPageSelected(int position) {
    if (pnListener == null) {
      return;
    }

    if (position == 0) {
      pnListener.onPrevious();
    } else if (position == 2) {
      pnListener.onNext();
    }
  }

  @Override
  public void onPageScrollStateChanged(int state) {
    if (state == ViewPager.SCROLL_STATE_IDLE) {
      setBackground(R.drawable.cover_unknown);
      Log.d(LOGTAG, "Changing page");
      pager.setCurrentItem(1, false);
    }
  }
}

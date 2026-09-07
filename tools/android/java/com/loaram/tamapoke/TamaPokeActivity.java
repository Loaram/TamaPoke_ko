package com.loaram.tamapoke;

import android.Manifest;
import android.app.NativeActivity;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkRequest;
import android.net.wifi.WifiManager;
import android.util.Log;
import java.net.Inet4Address;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.Toast;

public final class TamaPokeActivity extends NativeActivity {
    private static final int LOCAL_NETWORK_REQUEST = 38631;
    // All callback/session changes are synchronized. Native polling reads through
    // synchronized getters, never through a callback-owned Network object.
    private ConnectivityManager.NetworkCallback lanCallback;
    private Network lanNetwork;
    private WifiManager.MulticastLock lanMulticast;
    private int lanState; // 0 idle, 1 waiting, 2 IPv4 ready, -1 unavailable, -2 permission/error
    private int lanIpv4, lanBroadcast, lanEpoch;

    public synchronized int getLanState() { return lanState; }
    public synchronized int getLanIpv4() { return lanIpv4; }
    public synchronized int getLanBroadcast() { return lanBroadcast; }
    public synchronized int getLanEpoch() { return lanEpoch; }

    private void updateLanAddress(LinkProperties properties) {
        int ip = 0, broadcast = 0;
        if (properties != null) for (LinkAddress address : properties.getLinkAddresses()) {
            if (!(address.getAddress() instanceof Inet4Address)) continue;
            int prefix = address.getPrefixLength();
            if (prefix < 1 || prefix > 30) continue;
            byte[] b = address.getAddress().getAddress();
            ip = ((b[0] & 255) << 24) | ((b[1] & 255) << 16)
                    | ((b[2] & 255) << 8) | (b[3] & 255);
            broadcast = ip | (-1 >>> prefix);
            break;
        }
        if (ip != lanIpv4 || broadcast != lanBroadcast) ++lanEpoch;
        lanIpv4 = ip;
        lanBroadcast = broadcast;
        lanState = ip != 0 ? 2 : 1;
    }

    public synchronized boolean beginLanNetwork() {
        if (lanCallback != null) return true;
        final ConnectivityManager cm = getSystemService(ConnectivityManager.class);
        lanState = 1;
        lanIpv4 = lanBroadcast = 0;
        if (cm == null) { lanState = -1; return false; }
        final ConnectivityManager.NetworkCallback callback = new ConnectivityManager.NetworkCallback() {
            @Override public void onAvailable(Network network) {
                synchronized (TamaPokeActivity.this) {
                    if (lanCallback != this) return; // stale callback after leave/retry
                    if (!cm.bindProcessToNetwork(network)) { lanState = -1; return; }
                    lanNetwork = network;
                    lanIpv4 = lanBroadcast = 0;
                    ++lanEpoch;
                    // Wait for onLinkPropertiesChanged; onAvailable does not yet
                    // guarantee that querying LinkProperties returns this network.
                    lanState = 1;
                    Log.i("TamaPoke-LAN", "Wi-Fi network selected");
                }
            }
            @Override public void onLinkPropertiesChanged(Network network, LinkProperties properties) {
                synchronized (TamaPokeActivity.this) {
                    if (lanCallback == this && network.equals(lanNetwork)) updateLanAddress(properties);
                }
            }
            @Override public void onLost(Network network) {
                synchronized (TamaPokeActivity.this) {
                    if (lanCallback != this || !network.equals(lanNetwork)) return;
                    cm.bindProcessToNetwork(null);
                    lanNetwork = null;
                    lanIpv4 = lanBroadcast = 0;
                    ++lanEpoch;
                    lanState = -1;
                }
            }
            @Override public void onUnavailable() {
                synchronized (TamaPokeActivity.this) {
                    if (lanCallback == this) lanState = -1;
                }
            }
        };
        lanCallback = callback;
        try {
            WifiManager wifi = (WifiManager)getApplicationContext().getSystemService(WIFI_SERVICE);
            if (wifi != null) {
                lanMulticast = wifi.createMulticastLock("TamaPoke-LAN");
                lanMulticast.setReferenceCounted(false);
                lanMulticast.acquire();
            }
            // Deliberately no INTERNET capability: the ESP room has no Internet.
            cm.requestNetwork(new NetworkRequest.Builder()
                    .addTransportType(NetworkCapabilities.TRANSPORT_WIFI).build(), callback, 20000);
            return true;
        } catch (RuntimeException error) {
            Log.w("TamaPoke-LAN", "Wi-Fi request failed", error);
            endLanNetwork();
            lanState = -2;
            return false;
        }
    }

    public synchronized boolean endLanNetwork() {
        ConnectivityManager.NetworkCallback old = lanCallback;
        lanCallback = null; // invalidate queued callbacks before releasing anything
        ConnectivityManager cm = getSystemService(ConnectivityManager.class);
        if (cm != null) {
            if (old != null) try { cm.unregisterNetworkCallback(old); }
                catch (IllegalArgumentException ignored) { /* already unavailable */ }
            if (lanNetwork != null) cm.bindProcessToNetwork(null);
        }
        lanNetwork = null;
        if (lanMulticast != null && lanMulticast.isHeld()) lanMulticast.release();
        lanMulticast = null;
        lanState = lanIpv4 = lanBroadcast = 0;
        ++lanEpoch;
        return true;
    }

    @Override protected void onDestroy() {
        endLanNetwork();
        super.onDestroy();
    }

    @Override protected void onStop() {
        // Native ticking pauses in the background. Do not retain the watch's
        // Wi-Fi/filter lock for an invisible transfer; resuming requires retry.
        endLanNetwork();
        super.onStop();
    }

    public boolean showSaveReadError() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(TamaPokeActivity.this,
                        "저장 파일을 읽지 못했습니다. 원본은 보존됩니다. 앱 데이터를 지우지 마세요.",
                        Toast.LENGTH_LONG).show();
            }
        });
        return true;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        hideSystemBars();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) hideSystemBars();
    }

    public boolean hasLocalNetworkPermission() {
        if (Build.VERSION.SDK_INT < 37
                || checkSelfPermission(Manifest.permission.ACCESS_LOCAL_NETWORK)
                        == PackageManager.PERMISSION_GRANTED) {
            return true;
        }
        return false;
    }

    public boolean ensureLocalNetworkPermission() {
        if (hasLocalNetworkPermission()) return true;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                requestPermissions(
                        new String[] {Manifest.permission.ACCESS_LOCAL_NETWORK},
                        LOCAL_NETWORK_REQUEST);
            }
        });
        return false;
    }

    public int getBatteryPercent() {
        Intent status = registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (status == null) return -1;
        int level = status.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int scale = status.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (level < 0 || scale <= 0) return -1;
        return Math.max(0, Math.min(100, Math.round(level * 100.0f / scale)));
    }

    public boolean isBatteryCharging() {
        Intent status = registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (status == null) return false;
        int state = status.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
        return state == BatteryManager.BATTERY_STATUS_CHARGING
                || state == BatteryManager.BATTERY_STATUS_FULL;
    }

    private void hideSystemBars() {
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }
}

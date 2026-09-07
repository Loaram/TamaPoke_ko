#!/usr/bin/env python3
"""Execute the actual Activity's Wi-Fi callbacks against a deterministic fake OS.

This checks lifecycle/order/resource handling, not a real watch radio/driver.
Generated Android test doubles and class files stay in build/.
"""
import argparse
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STUBS = {
    'android/Manifest.java': '''package android; public class Manifest { public static class permission { public static final String ACCESS_LOCAL_NETWORK="local"; } }''',
    'android/content/Context.java': '''package android.content;
public class Context {
 public static final String WIFI_SERVICE="wifi";
 public static android.net.ConnectivityManager cm=new android.net.ConnectivityManager();
 public static android.net.wifi.WifiManager wifi=new android.net.wifi.WifiManager();
 public <T> T getSystemService(Class<T> cls){return cls.cast(cm);}
 public Object getSystemService(String s){return wifi;}
 public Context getApplicationContext(){return this;}
}''',
    'android/app/NativeActivity.java': '''package android.app;
public class NativeActivity extends android.content.Context {
 protected void onCreate(android.os.Bundle b){} protected void onDestroy(){} protected void onStop(){}
 public void onWindowFocusChanged(boolean b){}
 public int checkSelfPermission(String p){return 0;}
 public void requestPermissions(String[] p,int n){}
 public void runOnUiThread(Runnable r){r.run();}
 public android.content.Intent registerReceiver(Object o,android.content.IntentFilter f){return null;}
 public android.view.Window getWindow(){return new android.view.Window();}
}''',
    'android/content/Intent.java': '''package android.content; public class Intent { public static final String ACTION_BATTERY_CHANGED="battery"; public int getIntExtra(String s,int n){return n;} }''',
    'android/content/IntentFilter.java': '''package android.content; public class IntentFilter { public IntentFilter(String s){} }''',
    'android/content/pm/PackageManager.java': '''package android.content.pm; public class PackageManager { public static final int PERMISSION_GRANTED=0; }''',
    'android/os/Bundle.java': '''package android.os; public class Bundle {}''',
    'android/os/Build.java': '''package android.os; public class Build { public static class VERSION { public static int SDK_INT=30; } }''',
    'android/os/BatteryManager.java': '''package android.os; public class BatteryManager {
 public static final String EXTRA_LEVEL="level",EXTRA_SCALE="scale",EXTRA_STATUS="status";
 public static final int BATTERY_STATUS_CHARGING=1,BATTERY_STATUS_FULL=2;
}''',
    'android/view/Window.java': '''package android.view; public class Window { public WindowInsetsController getInsetsController(){return null;} public View getDecorView(){return new View();} }''',
    'android/view/View.java': '''package android.view; public class View {
 public static final int SYSTEM_UI_FLAG_IMMERSIVE_STICKY=1,SYSTEM_UI_FLAG_FULLSCREEN=2,SYSTEM_UI_FLAG_HIDE_NAVIGATION=4,SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN=8,SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION=16,SYSTEM_UI_FLAG_LAYOUT_STABLE=32;
 public void setSystemUiVisibility(int n){}
}''',
    'android/view/WindowInsetsController.java': '''package android.view; public class WindowInsetsController { public static final int BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE=1; public void hide(int n){} public void setSystemBarsBehavior(int n){} }''',
    'android/view/WindowInsets.java': '''package android.view; public class WindowInsets { public static class Type { public static int statusBars(){return 1;} public static int navigationBars(){return 2;} } }''',
    'android/widget/Toast.java': '''package android.widget; public class Toast { public static final int LENGTH_LONG=1; public static Toast makeText(Object c,String s,int n){return new Toast();} public void show(){} }''',
    'android/util/Log.java': '''package android.util; public class Log { public static int i(String t,String s){return 0;} public static int w(String t,String s,Throwable e){return 0;} }''',
    'android/net/Network.java': '''package android.net; public class Network {}''',
    'android/net/NetworkCapabilities.java': '''package android.net; public class NetworkCapabilities { public static final int TRANSPORT_WIFI=1; }''',
    'android/net/NetworkRequest.java': '''package android.net; public class NetworkRequest {
 public int transport;
 public static class Builder { private int t; public Builder addTransportType(int n){t=n;return this;} public NetworkRequest build(){NetworkRequest r=new NetworkRequest();r.transport=t;return r;} }
}''',
    'android/net/LinkAddress.java': '''package android.net; public class LinkAddress {
 private java.net.InetAddress address; private int prefix;
 public LinkAddress(String s,int p) throws Exception {address=java.net.InetAddress.getByName(s);prefix=p;}
 public java.net.InetAddress getAddress(){return address;} public int getPrefixLength(){return prefix;}
}''',
    'android/net/LinkProperties.java': '''package android.net; public class LinkProperties {
 public java.util.List<LinkAddress> addresses=new java.util.ArrayList<>();
 public java.util.List<LinkAddress> getLinkAddresses(){return addresses;}
}''',
    'android/net/ConnectivityManager.java': '''package android.net; public class ConnectivityManager {
 public NetworkCallback callback; public Network bound; public int requests,releases,timeout; public boolean rejectRequest,rejectBind;
 public boolean bindProcessToNetwork(Network n){if(rejectBind&&n!=null)return false;bound=n;return true;}
 public void requestNetwork(NetworkRequest r,NetworkCallback c,int ms){
  if(rejectRequest)throw new SecurityException("test denied");
  if(r.transport!=NetworkCapabilities.TRANSPORT_WIFI)throw new AssertionError("not Wi-Fi");
  callback=c;timeout=ms;requests++;
 }
 public void unregisterNetworkCallback(NetworkCallback c){releases++;if(callback==c)callback=null;}
 public static class NetworkCallback { public void onAvailable(Network n){} public void onLinkPropertiesChanged(Network n,LinkProperties p){} public void onLost(Network n){} public void onUnavailable(){} }
}''',
    'android/net/wifi/WifiManager.java': '''package android.net.wifi; public class WifiManager {
 public MulticastLock last;
 public MulticastLock createMulticastLock(String s){last=new MulticastLock();return last;}
 public static class MulticastLock { public boolean held; public void setReferenceCounted(boolean b){} public void acquire(){held=true;} public boolean isHeld(){return held;} public void release(){held=false;} }
}''',
    'com/loaram/tamapoke/LanCallbackTest.java': '''package com.loaram.tamapoke;
import android.content.Context;
import android.net.*;
public class LanCallbackTest {
 static int count;
 static void check(boolean b,String s){if(!b)throw new AssertionError(s);++count;System.out.println("PASS "+s);}
 static LinkProperties ip(String s,int prefix) throws Exception {LinkProperties p=new LinkProperties();p.addresses.add(new LinkAddress(s,prefix));return p;}
 public static void main(String[] args) throws Exception {
  TamaPokeActivity a=new TamaPokeActivity();ConnectivityManager cm=Context.cm;
  check(a.beginLanNetwork()&&a.getLanState()==1,"begin waits for Wi-Fi");
  check(cm.timeout==20000&&Context.wifi.last.held,"request timeout and multicast acquisition");
  a.beginLanNetwork();check(cm.requests==1,"duplicate begin does not leak requests");
  ConnectivityManager.NetworkCallback old=cm.callback;Network n=new Network();old.onAvailable(n);
  check(cm.bound==n&&a.getLanState()==1&&a.getLanIpv4()==0,"socket must wait for LinkProperties after binding");
  old.onLinkPropertiesChanged(n,ip("192.168.10.7",24));
  check(a.getLanState()==2&&a.getLanIpv4()==0xc0a80a07&&a.getLanBroadcast()==0xc0a80aff,"IPv4 and directed /24 broadcast");
  int epoch=a.getLanEpoch();old.onLinkPropertiesChanged(n,ip("192.168.10.7",24));
  check(a.getLanEpoch()==epoch,"unrelated property updates do not break active socket");
  old.onLinkPropertiesChanged(n,ip("172.20.10.2",28));
  check(a.getLanBroadcast()==0xac140a0f&&a.getLanEpoch()!=epoch,"hotspot /28 subnet and address-change epoch");
  old.onLost(new Network());check(a.getLanState()==2,"unrelated network loss ignored");
  old.onLost(n);check(a.getLanState()==-1&&cm.bound==null&&a.getLanIpv4()==0,"Wi-Fi loss clears routing/address");
  a.endLanNetwork();check(!Context.wifi.last.held&&cm.callback==null,"leave releases callback and filter lock");
  a.beginLanNetwork();ConnectivityManager.NetworkCallback fresh=cm.callback;
  old.onAvailable(n);old.onLinkPropertiesChanged(n,ip("1.2.3.4",24));old.onUnavailable();
  check(a.getLanState()==1&&cm.bound==null,"late callbacks cannot resurrect an ended session");
  Network next=new Network();fresh.onAvailable(next);fresh.onLinkPropertiesChanged(next,ip("192.168.1.2",24));
  check(a.getLanState()==2,"retry can acquire a fresh network");
  a.onStop();check(a.getLanState()==0&&cm.bound==null&&!Context.wifi.last.held,"background releases Wi-Fi and binding");
  fresh.onAvailable(next);check(cm.bound==null,"background late callback ignored");
  a.beginLanNetwork();cm.callback.onUnavailable();check(a.getLanState()==-1,"request timeout is distinguishable from peer timeout");a.endLanNetwork();
  cm.rejectRequest=true;check(!a.beginLanNetwork()&&a.getLanState()==-2&&!Context.wifi.last.held,"permission/request exception cleans resources");cm.rejectRequest=false;
  a.beginLanNetwork();cm.rejectBind=true;cm.callback.onAvailable(n);check(a.getLanState()==-1,"binding failure never reports ready");cm.rejectBind=false;a.endLanNetwork();
  a.beginLanNetwork();cm.callback.onAvailable(n);cm.callback.onLinkPropertiesChanged(n,ip("::1",128));
  check(a.getLanState()==1&&a.getLanIpv4()==0,"IPv6-only network cannot falsely report IPv4 readiness");
  a.onDestroy();check(cm.bound==null&&!Context.wifi.last.held,"destroy releases pending IPv4 session");
  a.endLanNetwork();check(a.getLanState()==0,"cleanup is idempotent");
  System.out.println("PASS: "+count+" actual Activity callback checks (fake Android OS)");
 }
}''',
}


def main():
    parser = argparse.ArgumentParser()
    default_jdk = Path(r'C:\Program Files\Android\Android Studio\jbr') if os.name == 'nt' else Path(shutil.which('javac') or '/usr/bin/javac').resolve().parents[1]
    parser.add_argument('--jdk', type=Path, default=default_jdk)
    args = parser.parse_args()
    out = ROOT / 'build/lan-java-tests'
    classes = out / 'classes'
    classes.mkdir(parents=True, exist_ok=True)
    for relative, source in STUBS.items():
        path = out / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(source, encoding='utf-8')
    activity = ROOT / 'tools/android/java/com/loaram/tamapoke/TamaPokeActivity.java'
    suffix = '.exe' if os.name == 'nt' else ''
    subprocess.run([str(args.jdk / ('bin/javac' + suffix)), '-encoding', 'UTF-8', '-d', str(classes),
                    str(activity), *[str(out / p) for p in STUBS]], check=True)
    subprocess.run([str(args.jdk / ('bin/java' + suffix)), '-cp', str(classes),
                    'com.loaram.tamapoke.LanCallbackTest'], check=True)


if __name__ == '__main__':
    main()

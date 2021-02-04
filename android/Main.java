/*
 * Main.java - all meaningful code is here
 * Copyright (C) 2021 Sanjay Rao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
package com.survey7.udpdisplay;

import android.app.Activity;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.TextView;
import android.widget.ImageView;
import android.graphics.Bitmap;
import android.os.Handler;
import java.lang.Runnable;
import java.lang.Thread;
import android.net.wifi.WifiManager;
import android.net.wifi.WifiInfo;
import android.text.format.Formatter;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.net.DatagramSocket;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.Socket;
import java.net.ServerSocket;
import android.view.ViewTreeObserver;
import android.view.Window;
import android.os.PowerManager;
import android.graphics.BitmapFactory;

public class Main extends Activity {
	private Globals globals;
	private boolean isinit=false;
	private boolean isuirunning=false;
	private Handler handler=new Handler();
	private DatagramSocket DS=null;
	private DatagramPacket DP=null;
	private ServerSocket SS=null;
	private byte[] packetdata=new byte[508];
	private int[] unpacketdata=new int[(508-15)/3];
	private int udpport=8081;
	private int tcpport=8081;
	private ImageView iv;
	private TextView tv;
	private class CommVars {
		public boolean isudpthreadactive=false;
		public boolean istcpthreadactive=false;

		public boolean isdestroy=false;
		public boolean isdirty=false;
		public boolean isfb=false;
		public Bitmap fb;
	}
	private CommVars commvars=new CommVars();
	private Runnable redraw_runnable=new Runnable() { public void run() { redraw(); } };
	private PowerManager PM;
	private PowerManager.WakeLock wl;

	private void requestlayoutcallback() {
		final ImageView liv=iv;
		if (!isuirunning) return;
		iv.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() { 
        @Override 
        public void onGlobalLayout() {
					liv.getViewTreeObserver().removeGlobalOnLayoutListener(this);
					init();
//					ltv.setText("Got layout event");
        } 
    });
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		isinit=false;
		requestlayoutcallback();
	}
	@Override
	public void onCreate(Bundle savedInstanceState) {
		globals=(Globals)((globalcontainer)getApplicationContext()).globals;
		super.onCreate(savedInstanceState);
		if (!isTaskRoot()) { finish(); return; } // fixes Market double-start
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		PM=(PowerManager)getSystemService(POWER_SERVICE);
		wl=PM.newWakeLock(PowerManager.SCREEN_BRIGHT_WAKE_LOCK|PowerManager.ACQUIRE_CAUSES_WAKEUP, Main.class.getPackage().getName());

		int off;
		for (off=0;off<100;off++) {
			try {
				DS=new DatagramSocket(udpport+off);
				DS.setSoTimeout(5*1000);
				break;
			} catch (java.net.SocketException e) {
				Log.e(globals.DBG,"Error listening on udp port "+(udpport+off));
			}
		}
		DP=new DatagramPacket(packetdata,packetdata.length);
		udpport+=off;

		for (off=0;off<100;off++) {
			try {
				SS=new ServerSocket(tcpport+off);
				SS.setSoTimeout(5*1000);
				break;
			} catch (Exception e) {
				Log.e(globals.DBG,"Error listening on tcp port "+(tcpport+off));
			}
		} 
		tcpport+=off;
	}
	private void redraw() {
		if (!isinit) {
			if (init()) return;
		}
		synchronized(commvars) {
			if (commvars.isdirty) {
				iv.setImageBitmap(commvars.fb);
//				tv.setText("Copied bitmap");
				commvars.isdirty=false;
			}
		}
	}
	private boolean init() {
		if (isinit) return false;
		if (!isuirunning) return true;
		synchronized(commvars) {
			final int w=iv.getWidth(),h=iv.getHeight();
			commvars.fb=Bitmap.createBitmap(w,h,Bitmap.Config.ARGB_8888);
			commvars.isfb=true;
			commvars.isdirty=false;
			iv.setImageBitmap(commvars.fb);
		}
		isinit=true;
		Log.e(globals.DBG,"Init successful");
		return false;
	}
	@Override
	public void onPause() {
Log.e(globals.DBG,"onPause");
		isuirunning=false;
		wl.release();
		isinit=false;
		synchronized(commvars) {
			commvars.isdestroy=true;
		}
		super.onPause();
	}
	public String getipstring(int udpport, int tcpport) {
		WifiManager WM=(WifiManager)getSystemService(WIFI_SERVICE);
		WifiInfo WI=WM.getConnectionInfo();
		return Formatter.formatIpAddress(WI.getIpAddress())+':'+udpport+'/'+tcpport;
	}
	@Override
	public void onResume() {
		super.onResume();
		setContentView(R.layout.main);
		wl.acquire();
		isuirunning=true;
		iv=(ImageView)findViewById(R.id.imageview);
		tv=(TextView)findViewById(R.id.textview);
		final TextView ltv=tv;
		Log.e(globals.DBG,"Listening on ports "+getipstring(udpport,tcpport));
		tv.setText("Ip/port: "+getipstring(udpport,tcpport));
		tv.setOnClickListener(new View.OnClickListener() {
				@Override
				public void onClick(View v) {
				}
		});
		synchronized(commvars) {
			commvars.isdestroy=false;
			if (!commvars.isudpthreadactive) {
				ListenUDPThread lthread;
				lthread=new ListenUDPThread();
				lthread.start();
				Log.e(globals.DBG,"Started UDP thread");
				commvars.isudpthreadactive=true;
			}
			if (!commvars.istcpthreadactive) {
				ListenTCPThread lthread;
				lthread=new ListenTCPThread();
				lthread.start();
				Log.e(globals.DBG,"Started TCP thread");
				commvars.istcpthreadactive=true;
			}
		}
		requestlayoutcallback();
	}

	private void query_reply(int off, final InetAddress addr) {
		int w=0,h=0;
		final int port;
		port=(int)(packetdata[off+1]&0xff)<<8|(int)(packetdata[off+2]&0xff);
// handler.post(new Runnable() { public void run() { tv.setText("Received query addr:"+addr.toString()+",port:"+port); } });
		synchronized(commvars) {
			if (commvars.isfb) {
				w=commvars.fb.getWidth();
				h=commvars.fb.getHeight();
			}
		}
		packetdata[0]=1;
		packetdata[1]=(byte)((w>>8)&0xff);
		packetdata[2]=(byte)(w&0xff);
		packetdata[3]=(byte)((h>>8)&0xff);
		packetdata[4]=(byte)(h&0xff);
		DatagramPacket p=new DatagramPacket(packetdata,5,addr,port);
		try {
			DS.send(p);
		} catch (Exception e)  {
// handler.post(new Runnable() { public void run() { tv.setText("Send exception addr:"+addr.toString()+",port:"+port); } });
		}
	}
	private void draw_reply(int off, final int len, final InetAddress addr) {
// 1:2,2:port,4:uid,2:left,2:top,2:width,2:height,3*width*height:RGBRGB...
		final byte uid0,uid1,uid2,uid3;
		final int left,top,width,height;
		final int port;
		off+=1; // 2
		port=(int)(packetdata[off]&0xff)<<8|(int)(packetdata[off+1]&0xff); off+=2;
		uid0=packetdata[off]; uid1=packetdata[off+1]; uid2=packetdata[off+2]; uid3=packetdata[off+3]; off+=4;
		left=(int)(packetdata[off]&0xff)<<8|(int)(packetdata[off+1]&0xff); off+=2;
		top=(int)(packetdata[off]&0xff)<<8|(int)(packetdata[off+1]&0xff); off+=2;
		width=(int)(packetdata[off]&0xff)<<8|(int)(packetdata[off+1]&0xff); off+=2;
		height=(int)(packetdata[off]&0xff)<<8|(int)(packetdata[off+1]&0xff); off+=2;
		if (15+3*width*height!=len) {
// handler.post(new Runnable() { public void run() { tv.setText("Received draw size:"+len+",expecting:"+(15+3*width*height)); } });
			return;
		}	
// handler.post(new Runnable() { public void run() { tv.setText("Received draw addr:"+addr.toString()+",port:"+port); } });
		boolean isdirty;
		synchronized(commvars) {
			isdirty=commvars.isdirty;
			commvars.isdirty=true;
			try {
				final int wxh=width*height;
				for (int uoff=0;uoff<wxh;uoff++) {
					int c=0xff000000;
					c|=((packetdata[off]&0xff)<<16);
					c|=((packetdata[off+1]&0xff)<<8);
					c|=(packetdata[off+2]&0xff);
					unpacketdata[uoff]=c;
					off+=3;
				}
				if (commvars.isfb) {
					commvars.fb.setPixels(unpacketdata,0,width,left,top,width,height);
				}
			} catch(Exception e) {}
		}
		if (!isdirty) handler.postDelayed(redraw_runnable,500);
		if (port==0) return;
		packetdata[0]=2; // ACK
		packetdata[1]=uid0;
		packetdata[2]=uid1;
		packetdata[3]=uid2;
		packetdata[4]=uid3;
		DatagramPacket p=new DatagramPacket(packetdata,5,addr,port);
		try {
			DS.send(p);
		} catch (Exception e)  {
// handler.post(new Runnable() { public void run() { tv.setText("Send exception addr:"+addr.toString()+",port:"+port); } });
		}
	}
	private void pixel_noreply(int off, final InetAddress addr) {
// 1:3,2:left,2:top,3:RGB
		final int left,top;
		off+=1;
		left=(int)(packetdata[off]&0xff)<<8|(int)(packetdata[off+1]&0xff); off+=2;
		top=(int)(packetdata[off]&0xff)<<8|(int)(packetdata[off+1]&0xff); off+=2;
		boolean isdirty;
		synchronized(commvars) {
			isdirty=commvars.isdirty;
			commvars.isdirty=true;
			int c=0xff000000;
			c|=((packetdata[off]&0xff)<<16);
			c|=((packetdata[off+1]&0xff)<<8);
			c|=(packetdata[off+2]&0xff);
			try { if (commvars.isfb) commvars.fb.setPixel(left,top,c); } catch(Exception e) {};
		}
		if (!isdirty) handler.post(redraw_runnable);
	}
	private void listenonudpport() {
		final int len,off;
		final InetAddress addr;
		DP.setData(packetdata); // is this needed?
		try {
			DS.receive(DP);
		} catch (java.io.IOException e) {
			return;
		}
		addr=DP.getAddress();
		len=DP.getLength();
		off=DP.getOffset();

//handler.post(new Runnable() { public void run() { tv.setText("Packet addr:"+addr.toString()+",len:"+len+",off:"+off+"packetdata[off]="+packetdata[off]); } });
		if ((len==3)&&(packetdata[off]==1)) { // query
			query_reply(off,addr);
		} else if ((len==8)&&(packetdata[off]==3)) {
			pixel_noreply(off,addr);
		} else if ((len>=15)&&(packetdata[off]==2)) {
			draw_reply(off,len,addr);
		}

//		boolean isdirty;
//		synchronized(commvars) {
//			isdirty=commvars.isdirty;
//			commvars.isdirty=true;
//			if (commvars.isfb) {
//				int i,j;
//				int w,h;
//				w=commvars.fb.getWidth();
//				h=commvars.fb.getHeight();
//				
//				int c=0xff000000;
//				c|=(packetdata[off]&0xff<<0);
//				c|=(packetdata[off]&0xff<<8);
//				c|=(packetdata[off]&0xff<<16);
//				for (i=0;i<w;i++) for (j=0;j<h;j++) commvars.fb.setPixel(i,j,c);
//			}
//		}
//		if (!isdirty) {
//			handler.post(redraw_runnable);
//		}
	}
	private void drawjpeg(int left, int top, byte[] payload, int size) throws Exception {
		Bitmap b;
		int[] pixels;
		final int w,h;
		try {
			b=BitmapFactory.decodeByteArray(payload,0,size);
			if (b==null) {
				Log.e(globals.DBG,"Bitmap is null");
				throw new NullPointerException();
			}
			w=b.getWidth(); h=b.getHeight();
			pixels=new int[w*h];
			b.getPixels(pixels,0,w,0,0,w,h);
			boolean isdirty;
			synchronized(commvars) {
				isdirty=commvars.isdirty;
				commvars.isdirty=true;
				if (commvars.isfb) {
					final int fbw=commvars.fb.getWidth(),fbh=commvars.fb.getHeight();
					if (left+w>fbw) left=fbw-w;
					if (top+h>fbh) top=fbh-h;
					commvars.fb.setPixels(pixels,0,w,left,top,w,h);
				}
			}
			if (!isdirty) handler.post(redraw_runnable);
		} catch(Exception e) {
			Log.e(globals.DBG,"Exception in drawjpeg");
			throw e;
		}
	}
	private boolean readn(InputStream is, byte[] dest, int n) throws Exception {
		int off=0;
		while (n!=0) {
			int i;
			try {
				i=is.read(dest,off,n);
			} catch(Exception e) {
				throw e;
			}
			if (i<0) return true;
			off+=i;
			n-=i;
		}
		return false;
	}
	private void listenontcpport() {
		Socket socket;
		InputStream is;
		OutputStream os;
		byte[] header=new byte[8];
		int left,top,jpegbytes;
		try {
			socket=SS.accept();
			is=socket.getInputStream();
			os=socket.getOutputStream();
			if (readn(is,header,8)) throw new IOException();
			left=(header[0]&0xFF)<<8|(header[1]&0xFF);
			top=(header[2]&0xFF)<<8|(header[3]&0xFF);
			jpegbytes= (header[4]&0xFF)<<24| (header[5]&0xFF)<<16| (header[6]&0xFF)<<8| (header[7]&0xFF);
			if (jpegbytes > 5*1000*1000) {
				Log.e(globals.DBG,"TCP Jpeg payload too large ("+jpegbytes+")>5M");
				throw new IllegalArgumentException("too big");
			} else if (jpegbytes!=0) {
				byte[] payload=new byte[jpegbytes];
				if (readn(is,payload,jpegbytes)) throw new IOException();
				drawjpeg(left,top,payload,jpegbytes);
				header[0]='O';
				header[1]='K';
				os.write(header,0,2);
			} else {
				int w=0,h=0;
				synchronized(commvars) {
					if (commvars.isfb) {
						w=commvars.fb.getWidth();
						h=commvars.fb.getHeight();
					}
				}
				header[0]=1;
				header[1]=(byte)((w>>8)&0xff);
				header[2]=(byte)(w&0xff);
				header[3]=(byte)((h>>8)&0xff);
				header[4]=(byte)(h&0xff);
				os.write(header,0,5);
			}
			socket.close();
		} catch(Exception e) {
		}
	}
	private class ListenUDPThread extends Thread {
		@Override
		public void run() {
			while(true) {
				listenonudpport(); // 5 second timeout
				synchronized(commvars) {
					if (commvars.isdestroy) {
						commvars.isudpthreadactive=false;
						return;
					}
				}
			}
		}
	}
	private class ListenTCPThread extends Thread {
		@Override
		public void run() {
			while(true) {
				listenontcpport(); // 5 second timeout
				synchronized(commvars) {
					if (commvars.isdestroy) {
						commvars.istcpthreadactive=false;
						return;
					}
				}
			}
		}
	}
}

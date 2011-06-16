#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <pulse/pulseaudio.h>

int32_t sounds[26*26][4096]; // 23.44Hz

char viewbuf[80*20];
int cursor=0;
int lastsound=0;

GCond* tickcond=0;
GMutex *tickmutex=0;

inline char gc() {
	char c=viewbuf[cursor++];
	printf("gc: '%c'\n",c);
	cursor%=80*20;
	return c;
}

int play[16]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};


//////////////////////////////////////////////////////////////////////////////////////////
// SOUND GENERATION
//////////////////////////////////////////////////////////////////////////////////////////

int32_t resample(int sndno,int note,int sampleno) {
	float r=exp2(note/12.0);
	float s=sampleno*r;
	float e=s+r;
	int is=((int)s);
	int es=e;
	int n=(es-is-1);

	int32_t *p=sounds[sndno];

	int32_t v=0;
	int i; for(i=is+1;i<es;i++) { v+=p[i%4096]; }
	float ps=1-(s-is);
	float pe=e-es;
	int32_t vs=p[is%4096] * ps;
	int32_t ve=p[es%4096] * pe;

	int ret=round((vs+v+ve)/(ps+n+pe));

	//printf("resample(%u %0.2f %0.2f(%u) %0.2f(%u)): %u %d %d ([%0.2f]%d+%d+%d[%0.2f])\n",
	//			sndno, r,s,is,e,es, sampleno,ret,n, ps,vs,v,ve,pe);
	return ret;
}

inline int16_t c2s16(char c) {
	const float M=('Z'-'A')+1;
	if(c>='a'&&c<='z') {
		return (INT16_MIN+1)*((c-'a'+1)/M);
	} else if(c>='A'&&c<='Z') {
		return INT16_MAX*((c-'A'+1)/M);
	} else {
		return 0;
	}
}

int generateSound(unsigned int len) {
	int i;
	int x=-1;
	int32_t y=0;
	for(i=0;i<4096;i++) {
		int nx=len*(i/4096.0);
		if(nx!=x) { y=c2s16(gc()); x=nx; }

		sounds[lastsound][i]=y;
	}
	return 0;
}


int gsnd() {
	unsigned int A=gc()-'A';
	unsigned int B=gc()-'A';
	unsigned int n=(A*26)|B;
	printf("read sound %u\n",n);
	if(B<0||B>676) return -1;
	return n;
}

int defineSound() {
	printf("define sound\n");
	int n=gsnd();
	if(n==-1) return -1;
	lastsound=n;

	int s=cursor;
	n=0;
	while(gc()!=' ') n++;
	cursor=s;
	return generateSound(n);
}

//////////////////////////////////////////////////////////////////////////////////////////
// AUDIO OUTPUT
//////////////////////////////////////////////////////////////////////////////////////////

static void pa_state_cb(pa_context *c, void *userdata) {
	pa_context_state_t state=pa_context_get_state(c);
	int *pa_ready=userdata;
	switch  (state) {
	case PA_CONTEXT_FAILED:
	case PA_CONTEXT_TERMINATED: *pa_ready=2; break;
	case PA_CONTEXT_READY: *pa_ready=1; break;
	default:;
	}
}

int ticksize=12000; // 1/8 second at 96kHz
static int offset=0;

static void audio_request_cb(pa_stream *s, size_t length, void *userdata) {
	int i;
	uint32_t buf[length/4];

	for(i=0;i<length/4;i++) {
		if((offset+i-1)/ticksize != (offset+i)/ticksize) {
			g_cond_signal(tickcond);
		}


		int k;
		int32_t v=0;
		for(k=0;k<16;k++) {
			int x=play[k];
			int note=x&0xff;
			if(note==0xff) continue;
			v+=resample(x>>8,x&0xff,offset+i);
		}
		buf[i]=v;
	}
	offset+=i;
	pa_stream_write(s,buf,length,0,0,PA_SEEK_RELATIVE);
}

static void audio_underflow_cb(pa_stream *s, void *userdata) {
	printf("underflow\n");
}


pa_stream *ps;

void audio_init() {
	pa_threaded_mainloop *pa_ml=pa_threaded_mainloop_new();
	pa_mainloop_api *pa_mlapi=pa_threaded_mainloop_get_api(pa_ml);
	pa_context *pa_ctx=pa_context_new(pa_mlapi, "te");
	pa_context_connect(pa_ctx, NULL, 0, NULL);
	int pa_ready = 0;
	pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);

	pa_threaded_mainloop_start(pa_ml);
	while(pa_ready==0) { ; }

	printf("audio ready\n");

	if (pa_ready == 2) {
		pa_context_disconnect(pa_ctx);
		pa_context_unref(pa_ctx);
		pa_threaded_mainloop_free(pa_ml);
	}

	pa_sample_spec ss;
	ss.rate=96000;
	ss.channels=1;
	ss.format=PA_SAMPLE_S24_32LE;
	ps=pa_stream_new(pa_ctx,"Playback",&ss,NULL);
	pa_stream_set_write_callback(ps,audio_request_cb,NULL);
	pa_stream_set_underflow_callback(ps,audio_underflow_cb,NULL);

	pa_buffer_attr bufattr;
	bufattr.fragsize = (uint32_t)-1;
	bufattr.maxlength = pa_usec_to_bytes(20000,&ss);
	bufattr.minreq = pa_usec_to_bytes(0,&ss);
	bufattr.prebuf = (uint32_t)-1;
	bufattr.tlength = pa_usec_to_bytes(20000,&ss);

	pa_stream_connect_playback(ps,NULL,&bufattr,
		PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_ADJUST_LATENCY|PA_STREAM_AUTO_TIMING_UPDATE|PA_STREAM_START_CORKED,NULL,NULL);
}

int setSound() {
	play[0]=(gsnd()<<8)|255;
	printf("setting sound %d\n",play[0]);
	return 0;
}

void playRawNote(uint8_t note) {
	play[0]=(play[0]&0xffffff00)|note;
}

int playNote(char c0) {
	int sharp=0;
	char c=gc();
	if(c=='#') { sharp=1; c=gc(); }
	unsigned int octave=c-'0';
	int n;

	printf("play note %c%s%c\n",c0,sharp?"#":"",c);

	if(sharp) {
		switch(c0) {
		case 'C': n=-5; break;
		case 'D': n=-3; break;
		case 'F': n=0; break;
		case 'G': n=2; break;
		case 'A': n=4; break;
		default: return -1;
		}
	} else {
		switch(c0) {
		case 'C': n=-6; break;
		case 'D': n=-4; break;
		case 'E': n=-2; break;
		case 'F': n=-1; break;
		case 'G': n=1; break;
		case 'A': n=3; break;
		case 'B': n=5; break;
		default: return -1;
		}
	}
	//  C   C# D  D#  E   F   F# G G# A A# B
	//  -6 -5 -4  -3 -2  -1   0  1 2  3 4  5

	int note=octave*12+n;

	printf("play note %c%s%c (%u)\n",c0,sharp?"#":"",c,note);

	// 0 is F#, A is 3
	playRawNote(note);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
// COMMANDS
//////////////////////////////////////////////////////////////////////////////////////////

int execute() {
	for(;;) {
		printf(".\n");
		char c;
		switch(c=gc()) {
		case 'd': defineSound(); break;
		case 's': setSound(); break;
		case ' ': break;
		case 'A'...'G': playNote(c); return 0; break;
		case '-': return 0; break;
		default: return -1;
		}
		if(cursor==0) return -1;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// VISUAL
//////////////////////////////////////////////////////////////////////////////////////////

static gboolean on_expose_event(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	cairo_t *cr = gdk_cairo_create (widget->window);
	PangoLayout *p=pango_cairo_create_layout(cr);
	PangoFontDescription *desc=pango_font_description_from_string("Courier 12");
	pango_layout_set_font_description(p,desc);
	pango_font_description_free(desc);

	int w,h;
	pango_layout_set_text(p,"0",1);
	pango_layout_get_size(p,&w,&h);
	w/=PANGO_SCALE;
	h/=PANGO_SCALE;

	if(cursor<20*80 && cursor>=0) {
		cairo_save(cr);
		cairo_set_source_rgb(cr,0,0.8,0);
		cairo_rectangle(cr,(cursor%80+1)*w,(cursor/80+1)*h,w,h);
		cairo_fill(cr);
		cairo_restore(cr);
	}

	cairo_move_to(cr,w,h);

	int i;
	for(i=0;i<20;i++) {
		pango_layout_set_text(p,viewbuf+80*i,80);
		pango_cairo_update_layout(cr,p);
		pango_cairo_show_layout(cr,p);
		cairo_rel_move_to(cr,0,h);
	}

	cairo_move_to(cr,w*81+w/2,h);
	cairo_rel_line_to(cr,0,h*20);
	cairo_line_to(cr,w,h*21);
	cairo_stroke(cr);


	for(i=0;i<512;i++) {
#ifndef VIEWA4
		int32_t v=sounds[lastsound][i*8];
		cairo_rectangle(cr,w+i,h*30, 1,5*h*(v/32767.0));
#else
		int32_t v=resample(lastsound,51,i);
		cairo_rectangle(cr,w+i,h*30, 1,5*h*(v/32767.0));
#endif
	}
	cairo_fill(cr);
	g_object_unref(p);


	cairo_destroy(cr);
	return FALSE;
}

static gboolean on_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	printf("pressed %s\n",event->string);

	if(event->state&GDK_MOD1_MASK) {
		switch(event->keyval) {
		//case GDK_p: start_sound((cursor/80)*80); break;
		case GDK_e: offset=0; pa_stream_cork(ps,0,0,0); break;
		}
	} else {
		switch(event->keyval) {
		case GDK_Escape: gtk_main_quit(); break;
		case GDK_Left: cursor--; break;
		case GDK_Right: cursor++; break;
		case GDK_Up: cursor-=80; break;
		case GDK_Down: cursor+=80; break;
		case GDK_Return: cursor=(cursor/80+1)*80;; break;
		case GDK_BackSpace: {
			int pos=cursor%80;
			if(pos>0) {
				memmove(viewbuf+cursor-1,viewbuf+cursor,80-pos);
				viewbuf[cursor+80-pos-1]=' ';
				cursor--;
				cursor%=(80*20);
			} else {
				cursor--;
				cursor%=(80*20);
				viewbuf[cursor]=' ';
			}
		} break;
		default:
			if(event->length==1) {
				int pos=cursor%80;
				memmove(viewbuf+cursor+1,viewbuf+cursor,80-pos-1);
				viewbuf[cursor++]=event->string[0];
			}
		}
	}

	printf("%d --\n",cursor);
	if(cursor<0) cursor=80*20+cursor;
	else cursor%=(80*20);
	printf("-- %d\n",cursor);

	gdk_window_invalidate_rect(gtk_widget_get_window(widget),0,1);

	return FALSE;
}


void load() {
	int i;
	FILE *f=fopen("test.snd","r");
	for(i=0;i<20;i++) {
		int j;
		for(j=0;j<80;j++) {
			int c=fgetc(f);
			if(c<1) return;

			if(c=='\n') break;
			viewbuf[i*80+j]=c;
		}
		for(;j<80;j++) {
			viewbuf[i*80+j]=' ';
		}
	}
	fclose(f);
}

GtkWidget *window;

gboolean update_view(gpointer _) {
	printf("update_view\n");
	gdk_window_invalidate_rect(gtk_widget_get_window(window),0,1);

	return FALSE;
}

GThread *tick_thread;

gpointer tick(gpointer _) {
	for(;;) {
		g_mutex_lock(tickmutex);
		g_cond_wait(tickcond,tickmutex);
		g_mutex_unlock(tickmutex);

		printf("tick %u\n",offset);
		g_idle_add(update_view,0);

		if(execute()==-1) {
			pa_stream_cork(ps,1,0,0);
			pa_stream_flush(ps,0,0);
			playRawNote(255);
			printf("stop");
		}
	}
	return 0;
}

int main(int argc,char *argv[])
{
	g_thread_init(0);

	memset(viewbuf,' ',sizeof(viewbuf));
	load();


	gtk_init(&argc,&argv);
	window=gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW (window),"te");

	tickcond=g_cond_new();
	tickmutex=g_mutex_new();
	tick_thread=g_thread_create(tick,0,0,0);

	g_signal_connect(window,"destroy",G_CALLBACK (gtk_main_quit),NULL);
	g_signal_connect(window,"key-press-event",G_CALLBACK(on_keypress),NULL);
	gtk_widget_add_events(window,GDK_KEY_PRESS_MASK);

	GtkWidget *a=gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(window),a);
	g_signal_connect(a,"expose-event",G_CALLBACK(on_expose_event),NULL);
	gtk_widget_show_all(window);

	audio_init();

	gtk_main();
	
	return 0;
}


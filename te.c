#include <stdio.h>
#include <math.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <pulse/pulseaudio.h>



//////////////////////////////////////////////////////////////////////////////////////////
// AUDIO
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

static void audio_request_cb(pa_stream *s, size_t length, void *userdata) {
	static int offset=0;
	printf("length %u\n",length);
	int i;
	short buf[length/2];
	for(i=0;i<length/2;i++) { buf[i]=10000*sin((offset+i)/50); }
	offset+=i;
	pa_stream_write(s,buf,length,0,0,PA_SEEK_RELATIVE);
}

static void audio_underflow_cb(pa_stream *s, void *userdata) {
	printf("underflow\n");
}


void audio_init() {
	pa_threaded_mainloop *pa_ml=pa_threaded_mainloop_new();
	pa_mainloop_api *pa_mlapi=pa_threaded_mainloop_get_api(pa_ml);
	pa_context *pa_ctx=pa_context_new(pa_mlapi, "te");
	pa_stream *ps;
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
	ss.rate=44100;
	ss.channels=1;
	ss.format=PA_SAMPLE_S16LE;
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
		PA_STREAM_INTERPOLATE_TIMING|PA_STREAM_ADJUST_LATENCY|PA_STREAM_AUTO_TIMING_UPDATE,NULL,NULL);
}

//////////////////////////////////////////////////////////////////////////////////////////
// VISUAL
//////////////////////////////////////////////////////////////////////////////////////////


int cursor;

char viewbuf[80*20];

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

	g_object_unref(p);


	cairo_destroy(cr);
	return FALSE;
}


static gboolean on_keypress(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	printf("pressed %s\n",event->string);

	switch(event->keyval) {
	case GDK_Escape: gtk_main_quit(); break;
	case GDK_BackSpace: cursor--; cursor%=(80*20); viewbuf[cursor]=' '; break;
	case GDK_Left: cursor--; break;
	case GDK_Right: cursor++; break;
	case GDK_Up: cursor-=80; break;
	case GDK_Down: cursor+=80; break;
	case GDK_Return: cursor=(cursor/80+1)*80;; break;
	default:
		if(event->length==1) {
			viewbuf[cursor++]=event->string[0];
		}
	}

	printf("%d --\n",cursor);
	if(cursor<0) cursor=80*20+cursor;
	else cursor%=(80*20);
	printf("-- %d\n",cursor);

	gdk_window_invalidate_rect(gtk_widget_get_window(widget),0,1);

	return FALSE;
}

int main(int argc,char *argv[])
{
	int i;
	for(i=0;i<80*20;i++) {viewbuf[i]=' ';}

	gtk_init(&argc,&argv);
	GtkWidget *window=gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW (window),"te");

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





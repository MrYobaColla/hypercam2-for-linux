#include <gtk/gtk.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#define APP_NAME "HyperCam 2 (Open Edition by Vlad Marmelad2001)"

typedef struct {
    GtkWidget *window;
    GtkWidget *notebook;
    
    GtkWidget *start_x_spin;
    GtkWidget *start_y_spin;
    GtkWidget *width_spin;
    GtkWidget *height_spin;
    GtkWidget *select_region_btn;
    GtkWidget *select_window_btn;
    GtkWidget *full_screen_btn;
    
    GtkWidget *start_key_entry;
    GtkWidget *pause_key_entry;
    GtkWidget *stop_key_entry;
    GtkWidget *enable_keys_check;
    GtkWidget *frame_shot_key_entry;
    GtkWidget *pan_lock_key_entry;
    
    GtkWidget *filename_entry;
    GtkWidget *browse_btn;
    GtkWidget *auto_naming_check;
    GtkWidget *quality_combo;
    GtkWidget *format_combo;
    GtkWidget *save_dir_entry;
    GtkWidget *browse_dir_btn;
    
    GtkWidget *sound_check;
    GtkWidget *system_audio_check;
    GtkWidget *microphone_check;
    GtkWidget *system_audio_combo;
    GtkWidget *microphone_combo;
    GtkWidget *volume_scale;
    GtkWidget *sample_rate_combo;
    
    GtkWidget *show_rect_check;
    GtkWidget *blink_rect_check;
    GtkWidget *window_opened_check;
    GtkWidget *iconize_check;
    GtkWidget *hide_check;
    GtkWidget *capture_layered_check;
    GtkWidget *frame_rate_combo;
    GtkWidget *cursor_capture_check;
    
    GtkWidget *license_text;
    
    GtkWidget *start_rec_btn;
    GtkWidget *start_paused_btn;
    GtkWidget *play_btn;
    GtkWidget *defaults_btn;
    GtkWidget *help_btn;
    
    gboolean is_recording;
    gboolean is_paused;
    GstElement *pipeline;
    guint screen_width;
    guint screen_height;
    
    guint start_key;
    guint pause_key;
    guint stop_key;
    gboolean keys_enabled;
    
    gchar *watermark_path;
    gchar *logo_path;
    gchar *save_dir;
    gchar *config_path;
    
} HyperCamApp;

static cairo_surface_t *watermark_surface = NULL;

static void draw_watermark(GstElement *overlay, cairo_t *cr, guint64 timestamp, guint64 duration, gpointer user_data) {
    if (watermark_surface && cairo_surface_status(watermark_surface) == CAIRO_STATUS_SUCCESS) {
        int img_width = cairo_image_surface_get_width(watermark_surface);
        int img_height = cairo_image_surface_get_height(watermark_surface);
        
        double target_width = 300;
        double target_height = 150;
        
        double scale_x = target_width / img_width;
        double scale_y = target_height / img_height;
        double scale = scale_x < scale_y ? scale_x : scale_y;
        
        double scaled_width = img_width * scale;
        double scaled_height = img_height * scale;
        
        double x = 0;
        double y = 0;
        
        cairo_save(cr);
        cairo_translate(cr, x, y);
        cairo_scale(cr, scale, scale);
        cairo_set_source_surface(cr, watermark_surface, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
    }
}

static void load_watermark(const gchar *filename) {
    if (watermark_surface) {
        cairo_surface_destroy(watermark_surface);
        watermark_surface = NULL;
    }
    if (filename && g_file_test(filename, G_FILE_TEST_EXISTS)) {
        watermark_surface = cairo_image_surface_create_from_png(filename);
    }
}

static void get_screen_dimensions(guint *width, guint *height) {
    GdkDisplay *display = gdk_display_get_default();
    GdkMonitor *monitor = gdk_display_get_primary_monitor(display);
    if (!monitor) {
        monitor = gdk_display_get_monitor(display, 0);
    }
    GdkRectangle geometry;
    gdk_monitor_get_geometry(monitor, &geometry);
    *width = geometry.width;
    *height = geometry.height;
}

static void select_region_clicked(GtkWidget *widget, gpointer data) {
    HyperCamApp *app = (HyperCamApp *)data;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_x_spin), 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_y_spin), 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->width_spin), 640);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->height_spin), 480);
}

static void select_window_clicked(GtkWidget *widget, gpointer data) {
    HyperCamApp *app = (HyperCamApp *)data;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_x_spin), 200);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_y_spin), 200);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->width_spin), 800);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->height_spin), 600);
}

static void full_screen_clicked(GtkWidget *widget, gpointer data) {
    HyperCamApp *app = (HyperCamApp *)data;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_x_spin), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_y_spin), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->width_spin), app->screen_width);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->height_spin), app->screen_height);
}

static void browse_filename_clicked(GtkWidget *widget, gpointer data) {
    HyperCamApp *app = (HyperCamApp *)data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save Video As", GTK_WINDOW(app->window), GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), app->save_dir);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "hypercam_recording.avi");
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "AVI files (*.avi)");
    gtk_file_filter_add_pattern(filter, "*.avi");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(app->filename_entry), filename);
        g_free(filename);
    }
    gtk_widget_destroy(dialog);
}

static void browse_dir_clicked(GtkWidget *widget, gpointer data) {
    HyperCamApp *app = (HyperCamApp *)data;
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Select Save Directory", GTK_WINDOW(app->window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "_Cancel", GTK_RESPONSE_CANCEL, "_Select", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), app->save_dir);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        gchar *dirname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(app->save_dir_entry), dirname);
        g_free(app->save_dir);
        app->save_dir = g_strdup(dirname);
        g_free(dirname);
    }
    gtk_widget_destroy(dialog);
}

static void save_config(HyperCamApp *app) {
    GKeyFile *keyfile = g_key_file_new();
    g_key_file_set_integer(keyfile, "screen", "start_x", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->start_x_spin)));
    g_key_file_set_integer(keyfile, "screen", "start_y", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->start_y_spin)));
    g_key_file_set_integer(keyfile, "screen", "width", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->width_spin)));
    g_key_file_set_integer(keyfile, "screen", "height", gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->height_spin)));
    g_key_file_set_string(keyfile, "hotkeys", "start_key", gtk_entry_get_text(GTK_ENTRY(app->start_key_entry)));
    g_key_file_set_string(keyfile, "hotkeys", "pause_key", gtk_entry_get_text(GTK_ENTRY(app->pause_key_entry)));
    g_key_file_set_boolean(keyfile, "hotkeys", "enabled", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->enable_keys_check)));
    g_key_file_set_string(keyfile, "avifile", "save_dir", app->save_dir);
    g_key_file_set_boolean(keyfile, "avifile", "auto_naming", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->auto_naming_check)));
    g_key_file_set_boolean(keyfile, "sound", "enabled", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->sound_check)));
    g_key_file_set_boolean(keyfile, "sound", "system_audio", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->system_audio_check)));
    g_key_file_set_boolean(keyfile, "sound", "microphone", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->microphone_check)));
    g_key_file_set_boolean(keyfile, "options", "show_rect", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->show_rect_check)));
    g_key_file_set_boolean(keyfile, "options", "blink_rect", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->blink_rect_check)));
    g_key_file_set_boolean(keyfile, "options", "capture_cursor", gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->cursor_capture_check)));
    gsize length;
    gchar *data = g_key_file_to_data(keyfile, &length, NULL);
    g_file_set_contents(app->config_path, data, length, NULL);
    g_free(data);
    g_key_file_free(keyfile);
}

static void load_config(HyperCamApp *app) {
    GKeyFile *keyfile = g_key_file_new();
    if (g_key_file_load_from_file(keyfile, app->config_path, G_KEY_FILE_NONE, NULL)) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_x_spin), g_key_file_get_integer(keyfile, "screen", "start_x", NULL));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_y_spin), g_key_file_get_integer(keyfile, "screen", "start_y", NULL));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->width_spin), g_key_file_get_integer(keyfile, "screen", "width", NULL));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->height_spin), g_key_file_get_integer(keyfile, "screen", "height", NULL));
        gchar *start_key = g_key_file_get_string(keyfile, "hotkeys", "start_key", NULL);
        if (start_key) gtk_entry_set_text(GTK_ENTRY(app->start_key_entry), start_key);
        gchar *pause_key = g_key_file_get_string(keyfile, "hotkeys", "pause_key", NULL);
        if (pause_key) gtk_entry_set_text(GTK_ENTRY(app->pause_key_entry), pause_key);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->enable_keys_check), g_key_file_get_boolean(keyfile, "hotkeys", "enabled", NULL));
        gchar *save_dir = g_key_file_get_string(keyfile, "avifile", "save_dir", NULL);
        if (save_dir) {
            g_free(app->save_dir);
            app->save_dir = save_dir;
            gtk_entry_set_text(GTK_ENTRY(app->save_dir_entry), save_dir);
        }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->auto_naming_check), g_key_file_get_boolean(keyfile, "avifile", "auto_naming", NULL));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->sound_check), g_key_file_get_boolean(keyfile, "sound", "enabled", NULL));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->system_audio_check), g_key_file_get_boolean(keyfile, "sound", "system_audio", NULL));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->microphone_check), g_key_file_get_boolean(keyfile, "sound", "microphone", NULL));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->show_rect_check), g_key_file_get_boolean(keyfile, "options", "show_rect", NULL));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->blink_rect_check), g_key_file_get_boolean(keyfile, "options", "blink_rect", NULL));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->cursor_capture_check), g_key_file_get_boolean(keyfile, "options", "capture_cursor", NULL));
    }
    g_key_file_free(keyfile);
}

static GstBusSyncReply bus_sync_handler(GstBus *bus, GstMessage *message, gpointer user_data) {
    HyperCamApp *app = (HyperCamApp *)user_data;
    
    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_EOS: {
            g_print("End of stream received - starting post-processing\n");
            
            gchar *video_file = g_object_get_data(G_OBJECT(app->pipeline), "video_file");
            gchar *system_audio_file = g_object_get_data(G_OBJECT(app->pipeline), "system_audio_file");
            gchar *microphone_file = g_object_get_data(G_OBJECT(app->pipeline), "microphone_file");
            gchar *final_file = g_object_get_data(G_OBJECT(app->pipeline), "final_file");
            gchar *temp_dir = g_object_get_data(G_OBJECT(app->pipeline), "temp_dir");
            
            gboolean record_sound = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(app->pipeline), "record_sound"));
            gboolean system_audio = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(app->pipeline), "system_audio"));
            gboolean microphone = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(app->pipeline), "microphone"));
            
            g_print("Video file exists: %d\n", g_file_test(video_file, G_FILE_TEST_EXISTS));
            if (system_audio_file) {
                g_print("System audio file exists: %d\n", g_file_test(system_audio_file, G_FILE_TEST_EXISTS));
            }
            if (microphone_file) {
                g_print("Microphone file exists: %d\n", g_file_test(microphone_file, G_FILE_TEST_EXISTS));
            }
            
            if (record_sound && (system_audio || microphone)) {
                g_print("Merging audio and video with ffmpeg...\n");
                
                GString *ffmpeg_cmd = g_string_new("ffmpeg -y ");
                
                g_string_append_printf(ffmpeg_cmd, "-i \"%s\" ", video_file);
                
                int audio_inputs = 0;
                if (system_audio && system_audio_file && g_file_test(system_audio_file, G_FILE_TEST_EXISTS)) {
                    g_string_append_printf(ffmpeg_cmd, "-i \"%s\" ", system_audio_file);
                    audio_inputs++;
                }
                if (microphone && microphone_file && g_file_test(microphone_file, G_FILE_TEST_EXISTS)) {
                    g_string_append_printf(ffmpeg_cmd, "-i \"%s\" ", microphone_file);
                    audio_inputs++;
                }
                
                g_string_append(ffmpeg_cmd, "-c:v copy ");
                
                if (audio_inputs == 2) {
                    g_string_append(ffmpeg_cmd, "-filter_complex \"[1:a][2:a]amix=inputs=2:duration=longest[a]\" -map 0:v -map \"[a]\" ");
                } else if (audio_inputs == 1) {
                    g_string_append(ffmpeg_cmd, "-map 0:v -map 1:a ");
                } else {
                    g_string_append(ffmpeg_cmd, "-map 0:v ");
                }
                
                g_string_append_printf(ffmpeg_cmd, "\"%s\"", final_file);
                
                g_print("FFmpeg command: %s\n", ffmpeg_cmd->str);
                
                int result = system(ffmpeg_cmd->str);
                if (result == 0) {
                    g_print("Successfully merged files: %s\n", final_file);
                } else {
                    g_print("Failed to merge files with ffmpeg\n");
                    g_print("Copying video only...\n");
                    if (g_file_test(video_file, G_FILE_TEST_EXISTS)) {
                        rename(video_file, final_file);
                    }
                }
                
                g_string_free(ffmpeg_cmd, TRUE);
            } else {
                g_print("Copying video file to final location...\n");
                if (g_file_test(video_file, G_FILE_TEST_EXISTS)) {
                    if (rename(video_file, final_file) == 0) {
                        g_print("Video saved: %s\n", final_file);
                    } else {
                        g_print("Failed to move video file\n");
                    }
                }
            }
            
            if (temp_dir && g_file_test(temp_dir, G_FILE_TEST_EXISTS)) {
                if (system_audio_file && g_file_test(system_audio_file, G_FILE_TEST_EXISTS)) {
                    remove(system_audio_file);
                }
                if (microphone_file && g_file_test(microphone_file, G_FILE_TEST_EXISTS)) {
                    remove(microphone_file);
                }
                if (g_file_test(video_file, G_FILE_TEST_EXISTS)) {
                    remove(video_file);
                }
                rmdir(temp_dir);
                g_print("Temporary files cleaned up\n");
            }
            
            GDateTime *now = g_object_get_data(G_OBJECT(app->pipeline), "now");
            if (now) {
                g_date_time_unref(now);
            }
            
            break;
        }
        case GST_MESSAGE_ERROR: {
            gchar *debug;
            GError *error;
            gst_message_parse_error(message, &error, &debug);
            g_print("Error: %s\n", error->message);
            g_error_free(error);
            g_free(debug);
            break;
        }
        default:
            break;
    }
    
    return GST_BUS_PASS;
}

static void start_recording_clicked(GtkWidget *widget, gpointer data) {
    HyperCamApp *app = (HyperCamApp *)data;
    if (app->is_recording) {
        g_print("Stopping recording and processing files...\n");
        
        GstElement *audio_pipeline = g_object_get_data(G_OBJECT(app->window), "audio_pipeline");
        if (audio_pipeline) {
            gst_element_set_state(audio_pipeline, GST_STATE_NULL);
            gst_object_unref(audio_pipeline);
            g_object_set_data(G_OBJECT(app->window), "audio_pipeline", NULL);
        }
        
        if (app->pipeline) {
            gst_element_set_state(app->pipeline, GST_STATE_NULL);
            
            gchar *video_file = g_object_get_data(G_OBJECT(app->pipeline), "video_file");
            gchar *system_audio_file = g_object_get_data(G_OBJECT(app->pipeline), "system_audio_file");
            gchar *microphone_file = g_object_get_data(G_OBJECT(app->pipeline), "microphone_file");
            gchar *final_file = g_object_get_data(G_OBJECT(app->pipeline), "final_file");
            gchar *temp_dir = g_object_get_data(G_OBJECT(app->pipeline), "temp_dir");
            gchar *timestamp = g_object_get_data(G_OBJECT(app->pipeline), "timestamp");
            GDateTime *now = g_object_get_data(G_OBJECT(app->pipeline), "now");
            
            gboolean record_sound = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(app->pipeline), "record_sound"));
            gboolean system_audio = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(app->pipeline), "system_audio"));
            gboolean microphone = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(app->pipeline), "microphone"));
            
            g_print("Video file exists: %d\n", g_file_test(video_file, G_FILE_TEST_EXISTS));
            if (system_audio_file) {
                g_print("System audio file exists: %d\n", g_file_test(system_audio_file, G_FILE_TEST_EXISTS));
            }
            if (microphone_file) {
                g_print("Microphone file exists: %d\n", g_file_test(microphone_file, G_FILE_TEST_EXISTS));
            }
            
            if (record_sound && (system_audio || microphone)) {
                g_print("Merging audio and video with ffmpeg...\n");
                
                GString *ffmpeg_cmd = g_string_new("ffmpeg -y ");
                
                g_string_append_printf(ffmpeg_cmd, "-i \"%s\" ", video_file);
                
                int audio_inputs = 0;
                if (system_audio && system_audio_file && g_file_test(system_audio_file, G_FILE_TEST_EXISTS)) {
                    g_string_append_printf(ffmpeg_cmd, "-i \"%s\" ", system_audio_file);
                    audio_inputs++;
                }
                if (microphone && microphone_file && g_file_test(microphone_file, G_FILE_TEST_EXISTS)) {
                    g_string_append_printf(ffmpeg_cmd, "-i \"%s\" ", microphone_file);
                    audio_inputs++;
                }
                
                g_string_append(ffmpeg_cmd, "-c:v libx264 -b:v 1000k -preset medium -crf 23 ");
                
                if (audio_inputs == 2) {
                    g_string_append(ffmpeg_cmd, "-filter_complex \"[1:a][2:a]amix=inputs=2:duration=longest[a]\" -map 0:v -map \"[a]\" ");
                } else if (audio_inputs == 1) {
                    g_string_append(ffmpeg_cmd, "-map 0:v -map 1:a ");
                } else {
                    g_string_append(ffmpeg_cmd, "-map 0:v ");
                }
                
                g_string_append_printf(ffmpeg_cmd, "\"%s\"", final_file);
                
                g_print("FFmpeg command: %s\n", ffmpeg_cmd->str);
                
                int result = system(ffmpeg_cmd->str);
                if (result == 0) {
                    g_print("Successfully merged files: %s\n", final_file);
                } else {
                    g_print("Failed to merge files with ffmpeg\n");
                    g_print("Copying video only...\n");
                    if (g_file_test(video_file, G_FILE_TEST_EXISTS)) {
                        rename(video_file, final_file);
                    }
                }
                
                g_string_free(ffmpeg_cmd, TRUE);
            } else {
                g_print("Copying video file to final location...\n");
                if (g_file_test(video_file, G_FILE_TEST_EXISTS)) {
                    if (rename(video_file, final_file) == 0) {
                        g_print("Video saved: %s\n", final_file);
                    } else {
                        g_print("Failed to move video file\n");
                    }
                }
            }
            
            if (temp_dir && g_file_test(temp_dir, G_FILE_TEST_EXISTS)) {
                if (system_audio_file && g_file_test(system_audio_file, G_FILE_TEST_EXISTS)) {
                    remove(system_audio_file);
                }
                if (microphone_file && g_file_test(microphone_file, G_FILE_TEST_EXISTS)) {
                    remove(microphone_file);
                }
                if (g_file_test(video_file, G_FILE_TEST_EXISTS)) {
                    remove(video_file);
                }
                rmdir(temp_dir);
                g_print("Temporary files cleaned up\n");
            }
            
            if (now) {
                g_date_time_unref(now);
            }
            
            gst_object_unref(app->pipeline);
            app->pipeline = NULL;
        }
        
        app->is_recording = FALSE;
        gtk_button_set_label(GTK_BUTTON(app->start_rec_btn), "Start Rec.");
    } else {
        gint start_x = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->start_x_spin));
        gint start_y = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->start_y_spin));
        gint width = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->width_spin));
        gint height = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app->height_spin));
        gboolean record_sound = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->sound_check));
        gboolean system_audio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->system_audio_check));
        gboolean microphone = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->microphone_check));
        
        GDateTime *now = g_date_time_new_now_local();
        gchar *timestamp = g_date_time_format(now, "%Y%m%d_%H%M%S");
        
        gchar *temp_dir = g_build_filename(g_get_home_dir(), ".hypercam2", "temp", NULL);
        g_mkdir_with_parents(temp_dir, 0755);
        
        gchar *video_file = g_strdup_printf("%s/video_%s.avi", temp_dir, timestamp);
        gchar *system_audio_file = NULL;
        gchar *microphone_file = NULL;
        gchar *final_file = g_strdup_printf("%s/hypercam_recording_%s.avi", app->save_dir, timestamp);
        
        if (record_sound && system_audio) {
            system_audio_file = g_strdup_printf("%s/system_audio_%s.wav", temp_dir, timestamp);
        }
        if (record_sound && microphone) {
            microphone_file = g_strdup_printf("%s/microphone_%s.wav", temp_dir, timestamp);
        }

        const gchar *pipeline_template;
        if (watermark_surface) {
            pipeline_template = 
                "ximagesrc startx=%d starty=%d endx=%d endy=%d use-damage=false ! "
                "videoconvert ! "
                "cairooverlay name=watermark ! "
                "videoconvert ! "
                "x264enc bitrate=1000 speed-preset=medium ! "
                "avimux ! "
                "filesink location=%s";
        } else {
            pipeline_template = 
                "ximagesrc startx=%d starty=%d endx=%d endy=%d use-damage=false ! "
                "videoconvert ! "
                "x264enc bitrate=1000 speed-preset=medium ! "
                "avimux ! "
                "filesink location=%s";
        }
        
        gchar *pipeline_str = g_strdup_printf(pipeline_template,
            start_x, start_y, start_x + width, start_y + height, video_file);

        g_print("Video pipeline: %s\n", pipeline_str);
        
        GError *error = NULL;
        app->pipeline = gst_parse_launch(pipeline_str, &error);
        g_free(pipeline_str);
        
        if (error) {
            g_print("Failed to create video pipeline: %s\n", error->message);
            g_error_free(error);
            g_free(video_file);
            g_free(system_audio_file);
            g_free(microphone_file);
            g_free(final_file);
            g_free(temp_dir);
            g_free(timestamp);
            g_date_time_unref(now);
            return;
        }
        
        if (watermark_surface) {
            GstElement *watermark = gst_bin_get_by_name(GST_BIN(app->pipeline), "watermark");
            if (watermark) {
                g_signal_connect(watermark, "draw", G_CALLBACK(draw_watermark), NULL);
                gst_object_unref(watermark);
            }
        }
        
        GstStateChangeReturn ret = gst_element_set_state(app->pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            g_print("Failed to start video pipeline\n");
            gst_object_unref(app->pipeline);
            app->pipeline = NULL;
        } else {
            app->is_recording = TRUE;
            gtk_button_set_label(GTK_BUTTON(app->start_rec_btn), "Stop Rec.");
            
            g_object_set_data_full(G_OBJECT(app->pipeline), "video_file", video_file, g_free);
            g_object_set_data_full(G_OBJECT(app->pipeline), "system_audio_file", system_audio_file, g_free);
            g_object_set_data_full(G_OBJECT(app->pipeline), "microphone_file", microphone_file, g_free);
            g_object_set_data_full(G_OBJECT(app->pipeline), "final_file", final_file, g_free);
            g_object_set_data_full(G_OBJECT(app->pipeline), "temp_dir", temp_dir, g_free);
            g_object_set_data_full(G_OBJECT(app->pipeline), "timestamp", timestamp, g_free);
            g_object_set_data(G_OBJECT(app->pipeline), "now", now);
            
            g_object_set_data(G_OBJECT(app->pipeline), "record_sound", GINT_TO_POINTER(record_sound));
            g_object_set_data(G_OBJECT(app->pipeline), "system_audio", GINT_TO_POINTER(system_audio));
            g_object_set_data(G_OBJECT(app->pipeline), "microphone", GINT_TO_POINTER(microphone));
            
            g_print("Started video recording: %s\n", video_file);
            
            if (record_sound && (system_audio || microphone)) {
                GString *audio_pipeline_str = g_string_new("");
                
                if (system_audio) {
                    g_string_append_printf(audio_pipeline_str,
                        "pulsesrc device=alsa_output.pci-0000_00_1f.3.analog-stereo.monitor ! "
                        "audioconvert ! "
                        "audio/x-raw,channels=2,rate=44100 ! "
                        "wavenc ! "
                        "filesink location=%s ",
                        system_audio_file);
                }
                
                if (microphone) {
                    g_string_append_printf(audio_pipeline_str,
                        "pulsesrc device=alsa_input.pci-0000_00_1f.3.analog-stereo ! "
                        "audioconvert ! "
                        "audio/x-raw,channels=2,rate=44100 ! "
                        "wavenc ! "
                        "filesink location=%s ",
                        microphone_file);
                }
                
                g_print("Audio pipeline: %s\n", audio_pipeline_str->str);
                
                GError *audio_error = NULL;
                GstElement *audio_pipeline = gst_parse_launch(audio_pipeline_str->str, &audio_error);
                g_string_free(audio_pipeline_str, TRUE);
                
                if (audio_error) {
                    g_print("Failed to create audio pipeline: %s\n", audio_error->message);
                    g_error_free(audio_error);
                } else {
                    GstStateChangeReturn audio_ret = gst_element_set_state(audio_pipeline, GST_STATE_PLAYING);
                    if (audio_ret == GST_STATE_CHANGE_FAILURE) {
                        g_print("Failed to start audio recording\n");
                        gst_object_unref(audio_pipeline);
                    } else {
                        g_object_set_data_full(G_OBJECT(app->window), "audio_pipeline", audio_pipeline, gst_object_unref);
                        g_print("Audio recording started\n");
                        if (system_audio) {
                            g_print("  System audio: %s\n", system_audio_file);
                        }
                        if (microphone) {
                            g_print("  Microphone: %s\n", microphone_file);
                        }
                    }
                }
            }
            
            g_print("Final output: %s\n", final_file);
        }
        
        save_config(app);
    }
}

static void start_audio_recording(HyperCamApp *app, const gchar *timestamp, const gchar *temp_dir) {
    gboolean record_sound = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->sound_check));
    gboolean system_audio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->system_audio_check));
    gboolean microphone = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(app->microphone_check));
    
    if (!record_sound) return;
    
    GString *audio_pipeline_str = g_string_new("");
    
    if (system_audio) {
        gchar *system_audio_file = g_strdup_printf("%s/system_audio_%s.wav", temp_dir, timestamp);
        g_string_append_printf(audio_pipeline_str,
            "pulsesrc device=alsa_output.pci-0000_00_1f.3.analog-stereo.monitor ! "
            "audioconvert ! "
            "audio/x-raw,channels=2,rate=44100 ! "
            "wavenc ! "
            "filesink location=%s ",
            system_audio_file);
        g_free(system_audio_file);
    }
    
    if (microphone) {
        gchar *microphone_file = g_strdup_printf("%s/microphone_%s.wav", temp_dir, timestamp);
        g_string_append_printf(audio_pipeline_str,
            "pulsesrc device=alsa_input.pci-0000_00_1f.3.analog-stereo ! "
            "audioconvert ! "
            "audio/x-raw,channels=2,rate=44100 ! "
            "wavenc ! "
            "filesink location=%s ",
            microphone_file);
        g_free(microphone_file);
    }
    
    if (audio_pipeline_str->len > 0) {
        g_print("Audio pipeline: %s\n", audio_pipeline_str->str);
        
        GError *error = NULL;
        GstElement *audio_pipeline = gst_parse_launch(audio_pipeline_str->str, &error);
        
        if (error) {
            g_print("Failed to create audio pipeline: %s\n", error->message);
            g_error_free(error);
        } else {
            GstStateChangeReturn ret = gst_element_set_state(audio_pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                g_print("Failed to start audio recording\n");
            } else {
                g_object_set_data_full(G_OBJECT(app->pipeline), "audio_pipeline", audio_pipeline, gst_object_unref);
                g_print("Audio recording started\n");
            }
        }
    }
    
    g_string_free(audio_pipeline_str, TRUE);
}

static void start_paused_clicked(GtkWidget *widget, gpointer data) {
    HyperCamApp *app = (HyperCamApp *)data;
    app->is_paused = !app->is_paused;
    if (app->pipeline) {
        if (app->is_paused) {
            gst_element_set_state(app->pipeline, GST_STATE_PAUSED);
            gtk_button_set_label(GTK_BUTTON(app->start_paused_btn), "Resume");
        } else {
            gst_element_set_state(app->pipeline, GST_STATE_PLAYING);
            gtk_button_set_label(GTK_BUTTON(app->start_paused_btn), "Start Paused");
        }
    }
}

static void detect_audio_devices(HyperCamApp *app) {
    g_print("Audio devices detection would go here\n");
}

static void play_clicked(GtkWidget *widget, gpointer data) {
    g_print("Play button clicked\n");
}

static void defaults_clicked(GtkWidget *widget, gpointer data) {
    HyperCamApp *app = (HyperCamApp *)data;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_x_spin), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_y_spin), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->width_spin), app->screen_width);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->height_spin), app->screen_height);
    gtk_entry_set_text(GTK_ENTRY(app->start_key_entry), "F2");
    gtk_entry_set_text(GTK_ENTRY(app->pause_key_entry), "F3");
    gtk_entry_set_text(GTK_ENTRY(app->frame_shot_key_entry), "F4");
    gtk_entry_set_text(GTK_ENTRY(app->pan_lock_key_entry), "Shift+F3");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->enable_keys_check), TRUE);
    gtk_entry_set_text(GTK_ENTRY(app->filename_entry), "hypercam_recording.avi");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->auto_naming_check), TRUE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->quality_combo), 2);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->format_combo), 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->sound_check), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->system_audio_check), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->microphone_check), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(app->volume_scale), 0.8);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->sample_rate_combo), 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->show_rect_check), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->blink_rect_check), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->window_opened_check), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->iconize_check), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->hide_check), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->capture_layered_check), FALSE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->frame_rate_combo), 2);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->cursor_capture_check), TRUE);
}

static void help_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "HyperCam 2 (Open Edition)\nCreated by Vlad Marmelad2001\nGitHub: https://github.com/MrYobaColla/hypercam2-for-linux");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static GtkWidget* create_screen_area_tab(HyperCamApp *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *frame, *hbox, *vbox_inner;
    
    frame = gtk_frame_new("Start Coordinates");
    vbox_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_inner), 10);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->start_x_spin = gtk_spin_button_new_with_range(0, 8192, 1);
    app->start_y_spin = gtk_spin_button_new_with_range(0, 8192, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_x_spin), 0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->start_y_spin), 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Start X"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->start_x_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Start Y"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->start_y_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox_inner);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    frame = gtk_frame_new("Dimensions");
    vbox_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_inner), 10);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->width_spin = gtk_spin_button_new_with_range(1, 8192, 1);
    app->height_spin = gtk_spin_button_new_with_range(1, 8192, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->width_spin), app->screen_width);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(app->height_spin), app->screen_height);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Width"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->width_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Height"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->height_spin, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(frame), vbox_inner);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->select_region_btn = gtk_button_new_with_label("Select Region");
    app->select_window_btn = gtk_button_new_with_label("Select Window");
    app->full_screen_btn = gtk_button_new_with_label("Full Screen");
    g_signal_connect(app->select_region_btn, "clicked", G_CALLBACK(select_region_clicked), app);
    g_signal_connect(app->select_window_btn, "clicked", G_CALLBACK(select_window_clicked), app);
    g_signal_connect(app->full_screen_btn, "clicked", G_CALLBACK(full_screen_clicked), app);
    gtk_box_pack_start(GTK_BOX(hbox), app->select_region_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->select_window_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->full_screen_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    return vbox;
}

static GtkWidget* create_hotkeys_tab(HyperCamApp *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *frame, *hbox, *vbox_inner;
    
    frame = gtk_frame_new("Recording Control");
    vbox_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_inner), 10);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->start_key_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->start_key_entry), "F2");
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Start/Stop Recording:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->start_key_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->pause_key_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->pause_key_entry), "F3");
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Pause/Resume:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->pause_key_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->frame_shot_key_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->frame_shot_key_entry), "F4");
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Single Frame Shot (in Pause mode):"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->frame_shot_key_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), vbox_inner);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    frame = gtk_frame_new("Pan Control");
    vbox_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_inner), 10);
    
    GtkWidget *label = gtk_label_new("Pan the capture area when the mouse is moved and the following keys are pressed:\nShift    Ctrl    Alt    Lock permanently");
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_inner), label, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->pan_lock_key_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->pan_lock_key_entry), "Shift+F3");
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Key to Switch Pan Lock:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->pan_lock_key_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), vbox_inner);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    app->enable_keys_check = gtk_check_button_new_with_label("Enable Hot Keys");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->enable_keys_check), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), app->enable_keys_check, FALSE, FALSE, 0);
    
    return vbox;
}

static GtkWidget* create_avi_file_tab(HyperCamApp *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *frame, *hbox, *vbox_inner;
    
    frame = gtk_frame_new("File Settings");
    vbox_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_inner), 10);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->filename_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->filename_entry), "hypercam_recording.avi");
    app->browse_btn = gtk_button_new_with_label("Browse");
    g_signal_connect(app->browse_btn, "clicked", G_CALLBACK(browse_filename_clicked), app);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("File Name:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->filename_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->browse_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->save_dir_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(app->save_dir_entry), app->save_dir);
    app->browse_dir_btn = gtk_button_new_with_label("Browse");
    g_signal_connect(app->browse_dir_btn, "clicked", G_CALLBACK(browse_dir_clicked), app);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Save Directory:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->save_dir_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->browse_dir_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    app->auto_naming_check = gtk_check_button_new_with_label("Use Automatic File Naming (hypercam_recording_YYYYMMDD_HHMMSS.avi)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->auto_naming_check), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_inner), app->auto_naming_check, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), vbox_inner);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    frame = gtk_frame_new("Quality Settings");
    vbox_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_inner), 10);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->quality_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->quality_combo), "Poor");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->quality_combo), "Medium");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->quality_combo), "Good");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->quality_combo), 2);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Video Quality:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->quality_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->format_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->format_combo), "AVI");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->format_combo), "MP4");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->format_combo), "MKV");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->format_combo), 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("File Format:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->format_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), vbox_inner);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    return vbox;
}

static GtkWidget* create_sound_tab(HyperCamApp *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *frame, *hbox, *vbox_inner;
    
    app->sound_check = gtk_check_button_new_with_label("Record Sound");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->sound_check), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), app->sound_check, FALSE, FALSE, 0);
    
    frame = gtk_frame_new("Audio Sources");
    vbox_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_inner), 10);
    
    app->system_audio_check = gtk_check_button_new_with_label("Record System Audio (Default Output)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->system_audio_check), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_inner), app->system_audio_check, FALSE, FALSE, 0);
    
    app->microphone_check = gtk_check_button_new_with_label("Record Microphone (Default Input)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->microphone_check), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox_inner), app->microphone_check, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), vbox_inner);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    frame = gtk_frame_new("Audio Settings");
    vbox_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_inner), 10);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.1);
    gtk_scale_set_value_pos(GTK_SCALE(app->volume_scale), 0.8);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Volume:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->volume_scale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->sample_rate_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->sample_rate_combo), "8000 Hz");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->sample_rate_combo), "44100 Hz");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->sample_rate_combo), "48000 Hz");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->sample_rate_combo), 1);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Sample Rate:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->sample_rate_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), vbox_inner);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    return vbox;
}

static GtkWidget* create_options_tab(HyperCamApp *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    
    app->show_rect_check = gtk_check_button_new_with_label("Show rectangle around recorded area");
    app->blink_rect_check = gtk_check_button_new_with_label("make this rectangle blink");
    app->window_opened_check = gtk_check_button_new_with_label("Leave HyperCam Window Opened");
    app->iconize_check = gtk_check_button_new_with_label("Iconize HyperCam Window to the Task Bar");
    app->hide_check = gtk_check_button_new_with_label("Hide HyperCam Window");
    app->capture_layered_check = gtk_check_button_new_with_label("Capture layered/transparent windows (may slow down performance)");
    app->cursor_capture_check = gtk_check_button_new_with_label("Capture mouse cursor");
    
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->show_rect_check), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->blink_rect_check), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->window_opened_check), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->iconize_check), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->hide_check), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->capture_layered_check), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(app->cursor_capture_check), TRUE);
    
    gtk_box_pack_start(GTK_BOX(vbox), app->show_rect_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), app->blink_rect_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), app->window_opened_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), app->iconize_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), app->hide_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), app->capture_layered_check, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), app->cursor_capture_check, FALSE, FALSE, 0);
    
    GtkWidget *frame = gtk_frame_new("Video Settings");
    GtkWidget *vbox_inner = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_inner), 10);
    
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    app->frame_rate_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->frame_rate_combo), "5 fps");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->frame_rate_combo), "10 fps");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->frame_rate_combo), "15 fps");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app->frame_rate_combo), "30 fps");
    gtk_combo_box_set_active(GTK_COMBO_BOX(app->frame_rate_combo), 2);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("Frame Rate:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->frame_rate_combo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_inner), hbox, FALSE, FALSE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), vbox_inner);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    return vbox;
}

static GtkWidget* create_license_tab(HyperCamApp *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    
    GtkWidget *logo_frame = gtk_frame_new(NULL);
    GtkWidget *logo_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(logo_box), 10);
    
    if (app->logo_path && g_file_test(app->logo_path, G_FILE_TEST_EXISTS)) {
        GtkWidget *logo_image = gtk_image_new_from_file(app->logo_path);
        gtk_image_set_pixel_size(GTK_IMAGE(logo_image), 64);
        gtk_box_pack_start(GTK_BOX(logo_box), logo_image, FALSE, FALSE, 0);
    }
    
    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *title_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_label), "<span size='x-large' weight='bold'>HyperCam 2 (Open Edition)</span>");
    gtk_box_pack_start(GTK_BOX(text_box), title_label, FALSE, FALSE, 0);
    
    GtkWidget *author_label = gtk_label_new("Created by Vlad Marmelad2001");
    gtk_box_pack_start(GTK_BOX(text_box), author_label, FALSE, FALSE, 0);
    
    GtkWidget *github_label = gtk_label_new("GitHub: https://github.com/MrYobaColla/hypercam2-for-linux");
    gtk_box_pack_start(GTK_BOX(text_box), github_label, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(logo_box), text_box, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(logo_frame), logo_box);
    gtk_box_pack_start(GTK_BOX(vbox), logo_frame, FALSE, FALSE, 0);
    
    GtkWidget *license_frame = gtk_frame_new("License");
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled_window), 200);
    
    app->license_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app->license_text), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app->license_text), GTK_WRAP_WORD);
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app->license_text));
    const gchar *license = 
        "MIT License\n\n"
        "Copyright (c) 2010 Vlad Marmelad2001\n\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n\n"
        "The above copyright notice and this permission notice shall be included in all\n"
        "copies or substantial portions of the Software.\n\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE.";
    
    gtk_text_buffer_set_text(buffer, license, -1);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), app->license_text);
    gtk_container_add(GTK_CONTAINER(license_frame), scrolled_window);
    gtk_box_pack_start(GTK_BOX(vbox), license_frame, TRUE, TRUE, 0);
    
    return vbox;
}

static void create_directories(HyperCamApp *app) {
    const gchar *home_dir = g_get_home_dir();
    gchar *hypercam_dir = g_build_filename(home_dir, ".hypercam2", NULL);
    g_mkdir_with_parents(hypercam_dir, 0755);
    
    app->watermark_path = g_build_filename(hypercam_dir, "yayko.png", NULL);
    app->logo_path = g_build_filename(hypercam_dir, "icon.png", NULL);
    app->save_dir = g_build_filename(home_dir, "Videos", NULL);
    app->config_path = g_build_filename(hypercam_dir, "conf.txt", NULL);
    
    g_mkdir_with_parents(app->save_dir, 0755);
    
    load_watermark(app->watermark_path);
    g_print("Watermark path: %s\n", app->watermark_path);
    g_print("Watermark loaded: %d\n", watermark_surface != NULL);
    
    g_free(hypercam_dir);
}

static void destroy_app(GtkWidget *widget, gpointer data) {
    HyperCamApp *app = (HyperCamApp *)data;
    
    GstElement *audio_pipeline = g_object_get_data(G_OBJECT(app->window), "audio_pipeline");
    if (audio_pipeline) {
        gst_element_set_state(audio_pipeline, GST_STATE_NULL);
        gst_object_unref(audio_pipeline);
    }
    
    if (app->pipeline) {
        gst_element_set_state(app->pipeline, GST_STATE_NULL);
        gst_object_unref(app->pipeline);
    }
    
    if (watermark_surface) {
        cairo_surface_destroy(watermark_surface);
    }
    
    save_config(app);
    g_free(app->watermark_path);
    g_free(app->logo_path);
    g_free(app->save_dir);
    g_free(app->config_path);
    g_free(app);
    
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    gst_init(&argc, &argv);
    
    HyperCamApp *app = g_new0(HyperCamApp, 1);
    app->is_recording = FALSE;
    app->is_paused = FALSE;
    
    create_directories(app);
    get_screen_dimensions(&app->screen_width, &app->screen_height);
    
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), APP_NAME);
    gtk_window_set_default_size(GTK_WINDOW(window), 450, 500);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    
    if (app->logo_path && g_file_test(app->logo_path, G_FILE_TEST_EXISTS)) {
        gtk_window_set_icon_from_file(GTK_WINDOW(window), app->logo_path, NULL);
    }
    
    app->window = window;
    g_signal_connect(window, "destroy", G_CALLBACK(destroy_app), app);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    GtkWidget *notebook = gtk_notebook_new();
    app->notebook = notebook;
    
    GtkWidget *screen_area_tab = create_screen_area_tab(app);
    GtkWidget *hotkeys_tab = create_hotkeys_tab(app);
    GtkWidget *avi_file_tab = create_avi_file_tab(app);
    GtkWidget *sound_tab = create_sound_tab(app);
    GtkWidget *options_tab = create_options_tab(app);
    GtkWidget *license_tab = create_license_tab(app);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), screen_area_tab, gtk_label_new("Screen Area"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), hotkeys_tab, gtk_label_new("Hot Keys"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), avi_file_tab, gtk_label_new("AVI File"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), sound_tab, gtk_label_new("Sound"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), options_tab, gtk_label_new("Options"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), license_tab, gtk_label_new("License"));
    
    gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
    
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    app->start_rec_btn = gtk_button_new_with_label("Start Rec.");
    app->start_paused_btn = gtk_button_new_with_label("Start Paused");
    app->play_btn = gtk_button_new_with_label("Play");
    app->defaults_btn = gtk_button_new_with_label("Defaults");
    app->help_btn = gtk_button_new_with_label("Help");
    
    g_signal_connect(app->start_rec_btn, "clicked", G_CALLBACK(start_recording_clicked), app);
    g_signal_connect(app->start_paused_btn, "clicked", G_CALLBACK(start_paused_clicked), app);
    g_signal_connect(app->play_btn, "clicked", G_CALLBACK(play_clicked), app);
    g_signal_connect(app->defaults_btn, "clicked", G_CALLBACK(defaults_clicked), app);
    g_signal_connect(app->help_btn, "clicked", G_CALLBACK(help_clicked), app);
    
    gtk_box_pack_start(GTK_BOX(hbox), app->start_rec_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->start_paused_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->play_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->defaults_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), app->help_btn, TRUE, TRUE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    GtkWidget *ad_label = gtk_label_new("Try AVS Video Editor! Cut and join HyperCam clips and other media, add transitions, effects, titles, music. Upload to YouTube, turn to DVD!");
    gtk_label_set_line_wrap(GTK_LABEL(ad_label), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), ad_label, FALSE, FALSE, 0);
    
    load_config(app);
    gtk_widget_show_all(window);
    
    gtk_main();
    return 0;
}

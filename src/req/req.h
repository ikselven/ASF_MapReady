#ifndef __REQ_H
#define __REQ_H

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "asf.h"

/* for win32, need __declspec(dllexport) on all signal handlers. */
#if !defined(SIGNAL_CALLBACK)
#  if defined(win32)
#    define SIGNAL_CALLBACK __declspec(dllexport)
#  else
#    define SIGNAL_CALLBACK
#  endif
#endif

/********************************** Settings ********************************/
typedef struct
{
    char *csv_dir;
    char *output_dir;
    int req_num;
}
Settings;

/********************************** Prototypes ******************************/

/* font.c */
void set_font(void);

/* utility.c */
void add_to_combobox(const char *widget_name, const char *txt);
void set_combo_box_item_checked(const char *widget_name, gint index);
void rb_select(const char *widget_name, gboolean is_on);
double get_double_from_entry(const char *widget_name);
void put_double_to_entry(const char *widget_name, double val);
int get_int_from_entry(const char *widget_name);
void put_int_to_entry(const char *widget_name, int val);
int get_checked(const char *widget_name);
void set_checked(const char *widget_name, int checked);
void message_box(const char *format, ...);
GtkWidget *get_widget_checked(const char *widget_name);
void set_combobox_entry_maxlen(const char *widget_name, int maxlen);
char* get_string_from_entry(const char *widget_name);
void put_string_to_entry(const char *widget_name, char *txt);
char *get_string_from_comboboxentry(const char *widget_name);
void put_string_to_comboboxentry(const char *widget_name, char *txt);

/* req.c */
char *find_in_share(const char * filename);

/* settings.c */
void apply_saved_settings();
char *settings_get_output_dir();
char *settings_get_csv_dir();

/* csv_list.c */
void populate_csvs();

/* process.c */
void process();

#ifdef win32
#ifdef DIR_SEPARATOR
#undef DIR_SEPARATOR
#endif
extern const char DIR_SEPARATOR;
#endif

extern const char PATH_SEPATATOR;

/*************** these are our global variables ... ***********************/

/* xml version of the .glade file */
extern GladeXML *glade_xml;

#endif

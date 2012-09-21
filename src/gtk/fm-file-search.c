/*
 * file-search-ui.c
 * 
 * Copyright 2010 Shae Smittle <starfall87@gmail.com>
 * Copyright 2012 Hong Jen Yee (PCMan) <pcman.tw@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */
 
 #include "file-search.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libfm/fm-gtk.h>
#include <math.h>

#include "main-win.h"
#include "pcmanfm.h"

/* generated by glade-connect-gen, defined in file-search-ui.c */
static void filesearch_glade_connect_signals(GtkBuilder* builder, gpointer user_data);

typedef struct
{ 
    GtkDialog * dlg;
    GtkTreeView * path_tree_view;
    GtkEntry* name_entry;
    GtkCheckButton* name_case_insensitive_checkbutton;
    GtkCheckButton* name_regex_checkbutton;
    GtkCheckButton* search_recursive_checkbutton;
    GtkCheckButton* search_hidden_files_checkbutton;

    GtkCheckButton* text_file_checkbutton;
    GtkCheckButton* image_file_checkbutton;
    GtkCheckButton* audio_file_checkbutton;
    GtkCheckButton* video_file_checkbutton;
    GtkCheckButton* doc_file_checkbutton;

    GtkEntry* content_entry;
    GtkCheckButton* content_case_insensitive_checkbutton;
    GtkCheckButton* content_regex_checkbutton;

    GtkSpinButton* bigger_spinbutton;
    GtkComboBox* bigger_unit_combo;

    GtkSpinButton* smaller_spinbutton;
    GtkComboBox* smaller_unit_combo;

    GtkCheckButton* min_mtime_checkbutton;
    GtkButton* min_mtime_button;
    GtkCheckButton* max_mtime_checkbutton;
    GtkButton* max_mtime_button;

    GtkListStore * path_list_store;

    GtkDialog* date_dlg;
    GtkCalendar* calendar;
} FileSearchUI;

static gchar * document_mime_types[] = {
    "application/pdf",
    "application/msword",
    "application/vnd.ms-excel",
    "application/vnd.ms-powerpoint",
    "application/vnd.oasis.opendocument.chart",
    "application/vnd.oasis.opendocument.database",
    "application/vnd.oasis.opendocument.formula",
    "application/vnd.oasis.opendocument.graphics",
    "application/vnd.oasis.opendocument.image",
    "application/vnd.oasis.opendocument.presentation",
    "application/vnd.oasis.opendocument.spreadsheet",
    "application/vnd.oasis.opendocument.text",
    "application/vnd.oasis.opendocument.text-master",
    "application/vnd.oasis.opendocument.text-web",
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
    "application/vnd.openxmlformats-officedocument.presentationml.presentation",
    "application/vnd.openxmlformats-officedocument.presentationml.slideshow",
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
    "application/x-abiword",
    "application/x-gnumeric",
    "application/x-dvi",
    NULL
};
                         
/* UI Signal Handlers */

static gboolean launch_search(FileSearchUI* ui)
{
    GString* search_uri = g_string_sized_new(1024);
    GtkTreeModel* model;
    GtkTreeIter it;

    /* build the search:// URI to perform the search */
    g_string_append(search_uri, "search:/");

    model = GTK_TREE_MODEL(ui->path_list_store);
    if(gtk_tree_model_get_iter_first(model, &it)) /* we need to have at least one dir path */
    {
        gboolean recursive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->search_recursive_checkbutton));
        gboolean show_hidden = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->search_hidden_files_checkbutton));
        const char* name_patterns = gtk_entry_get_text(ui->name_entry);
        gboolean name_ci = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->name_case_insensitive_checkbutton));
        gboolean name_regex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->name_regex_checkbutton));
        const char* content_pattern = gtk_entry_get_text(ui->content_entry);
        gboolean content_ci = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->content_case_insensitive_checkbutton));
        gboolean content_regex = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->content_regex_checkbutton));
        const guint unit_bytes[] = {1, (1024), (1024*1024), (1024*1024*1024)};
        GSList* mime_types = NULL;
        FmPath* search_path;

        /* add paths */
        for(;;)
        {
            char* path_str;
            gtk_tree_model_get(model, &it, 0, &path_str, -1);

            /* FIXME: ths paths should be escaped */
            g_string_append(search_uri, path_str);

            g_free(path_str);

            if(!gtk_tree_model_iter_next(model, &it)) /* no more items */
                break;
            g_string_append_c(search_uri, ':'); /* separator for paths */
        }

        g_string_append_c(search_uri, '?');
        g_string_append_printf(search_uri, "recursive=%c", recursive ? '1' : '0');
        g_string_append_printf(search_uri, "&show_hidden=%c", show_hidden ? '1' : '0');
        if(name_patterns && *name_patterns)
        {
            g_string_append_printf(search_uri, "&name=%s", name_patterns);
            if(name_ci)
                g_string_append_printf(search_uri, "&name_ci=%c", name_ci ? '1' : '0');
        }

        if(content_pattern && *content_pattern)
        {
            if(content_regex)
                g_string_append_printf(search_uri, "&content_regex=%s", content_pattern);
            else
                g_string_append_printf(search_uri, "&content=%s", content_pattern);
            if(content_ci)
                g_string_append_printf(search_uri, "&content_ci=%c", content_ci ? '1' : '0');
        }

        /* search for the files of specific mime-types */
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->text_file_checkbutton)))
            mime_types = g_slist_prepend(mime_types, (gpointer)"text/plain");
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->image_file_checkbutton)))
            mime_types = g_slist_prepend(mime_types, (gpointer)"image/*");
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->audio_file_checkbutton)))
            mime_types = g_slist_prepend(mime_types, (gpointer)"audio/*");
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->video_file_checkbutton)))
            mime_types = g_slist_prepend(mime_types, (gpointer)"video/*");
        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->doc_file_checkbutton)))
        {
            // mime_types = g_slist_prepend(mime_types, (gpointer)"text/plain");
        }
        
        if(mime_types)
        {
            GSList* l;
            g_string_append(search_uri, "&mime_types=");
            for(l = mime_types; l; l=l->next)
            {
                const char* mime_type = (const char*)l->data;
                g_string_append(search_uri, mime_type);
                if(l->next)
                    g_string_append_c(search_uri, ';');
            }
            g_slist_free(mime_types);
        }

        if(gtk_widget_get_sensitive(GTK_WIDGET(ui->bigger_spinbutton)))
        {
            gdouble min_size = gtk_spin_button_get_value(ui->bigger_spinbutton);
            gint unit_index = gtk_combo_box_get_active(ui->bigger_unit_combo);
            /* convert to bytes */
            min_size *= unit_bytes[unit_index];
            g_string_append_printf(search_uri, "&min_size=%llu", (guint64)min_size);
        }

        if(gtk_widget_get_sensitive(GTK_WIDGET(ui->smaller_spinbutton)))
        {
            gdouble max_size = gtk_spin_button_get_value(ui->smaller_spinbutton);
            gint unit_index = gtk_combo_box_get_active(ui->smaller_unit_combo);
            /* convert to bytes */
            max_size *= unit_bytes[unit_index];
            g_string_append_printf(search_uri, "&min_size=%llu", (guint64)max_size);
        }

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->min_mtime_checkbutton)))
        {
            const char* label = gtk_button_get_label(ui->min_mtime_button);
            if(g_strcmp0(label, _("(None)")) != 0)
                g_string_append_printf(search_uri, "&min_mtime=%s", label);
        }

        if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->max_mtime_checkbutton)))
        {
            const char* label = gtk_button_get_label(ui->max_mtime_button);
            if(g_strcmp0(label, _("(None)")) != 0)
                g_string_append_printf(search_uri, "&max_mtime=%s", label);            
        }

        search_path = fm_path_new_for_uri(search_uri->str);

        fm_main_win_open_in_last_active(search_path);

        fm_path_unref(search_path);
        g_string_free(search_uri, TRUE);
    }
    else
    {
        /* show error if no paths are added */
        fm_show_error(GTK_WINDOW(ui->dlg), NULL, _("No folders are specified."));
        return FALSE;
    }
    return TRUE;
}

static void on_dlg_response(GtkDialog* dlg, int response, gpointer user_data)
{
    FileSearchUI * ui = (FileSearchUI *)user_data;

    if(response == GTK_RESPONSE_OK)
    {
        if(!launch_search(ui))
            return;
    }

    gtk_widget_destroy(GTK_WIDGET(ui->date_dlg));
    gtk_widget_destroy(GTK_WIDGET(dlg));
    pcmanfm_unref();
}

static void add_path(GtkWidget * list_store, const char* uri)
{
    char* filename;
    GtkTreeIter it;
    gtk_list_store_append(list_store, &it);
    filename = g_filename_from_uri(uri, NULL, NULL);
    if(filename)
    {
        gtk_list_store_set(list_store, &it, 0, filename, -1);
        g_free(filename);
    }
    else
        gtk_list_store_set(list_store, &it, 0, uri, -1);
}

static void on_add_path_button_clicked(GtkButton * btn, gpointer user_data)
{
    FileSearchUI * ui = (FileSearchUI *)user_data;
    GtkWidget* dlg = gtk_file_chooser_dialog_new(
                          _("Select a folder"), GTK_WINDOW(ui->dlg),
                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                          GTK_STOCK_OPEN, GTK_RESPONSE_OK,
                          NULL );
    gtk_dialog_set_alternative_button_order(GTK_DIALOG(dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dlg), TRUE);

    if(gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
    {
        GSList* uris = gtk_file_chooser_get_uris(GTK_FILE_CHOOSER(dlg));
        GSList* l;
        for(l = uris; l; l=l->next)
        {
            char* uri = (char*)l->data;
            add_path(ui->path_list_store, uri);
            g_free(uri);
        }
        g_slist_free(uris);
    }
    gtk_widget_destroy(dlg);
}

static void on_remove_path_button_clicked(GtkButton * btn, gpointer user_data)
{
    FileSearchUI * ui = (FileSearchUI *)user_data;

    GtkTreeIter it;
    GtkTreeSelection* sel = gtk_tree_view_get_selection( GTK_TREE_VIEW(ui->path_tree_view) );
    if( gtk_tree_selection_get_selected(sel, NULL, &it) )
        gtk_list_store_remove( ui->path_list_store, &it );
}

static void file_search_ui_free(FileSearchUI* ui)
{
    g_slice_free(FileSearchUI, ui);
}

void fm_search_file_in_folder(FmPath* folder_path)
{
    GList paths = {0};
    paths.data = folder_path;
    fm_search_file_in_folders(folder_path ? &paths : NULL);
}

void fm_search_file_in_folders(GList* folder_paths)
{
    FileSearchUI * ui = g_slice_new0(FileSearchUI);

    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/filesearch.ui", NULL);

    ui->dlg = GTK_DIALOG(gtk_builder_get_object(builder, "dlg"));
    gtk_dialog_set_alternative_button_order(GTK_DIALOG(ui->dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL);
    ui->path_tree_view = GTK_TREE_VIEW(gtk_builder_get_object(builder, "path_tree_view"));

    ui->name_entry = GTK_ENTRY(gtk_builder_get_object(builder, "name_entry"));
    ui->name_case_insensitive_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "name_case_insensitive_checkbutton"));
    ui->name_regex_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "name_regex_checkbutton"));
    ui->search_recursive_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "search_recursive_checkbutton"));
    ui->search_hidden_files_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "search_hidden_files_checkbutton"));

    ui->text_file_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "text_file_checkbutton"));
    ui->image_file_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "image_file_checkbutton"));
    ui->audio_file_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "audio_file_checkbutton"));
    ui->video_file_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "video_file_checkbutton"));
    ui->doc_file_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "doc_file_checkbutton"));

    ui->content_entry = GTK_ENTRY(gtk_builder_get_object(builder, "content_entry"));
    ui->content_case_insensitive_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "content_case_insensitive_checkbutton"));
    ui->content_regex_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "content_regex_checkbutton"));

    ui->bigger_spinbutton = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "bigger_spinbutton"));
    ui->bigger_unit_combo = GTK_COMBO_BOX(gtk_builder_get_object(builder, "bigger_unit_combo"));

    ui->smaller_spinbutton = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "smaller_spinbutton"));
    ui->smaller_unit_combo = GTK_COMBO_BOX(gtk_builder_get_object(builder, "smaller_unit_combo"));
    
    ui->min_mtime_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "min_mtime_checkbutton"));
    ui->min_mtime_button = GTK_BUTTON(gtk_builder_get_object(builder, "min_mtime_button"));
    ui->max_mtime_checkbutton = GTK_CHECK_BUTTON(gtk_builder_get_object(builder, "max_mtime_checkbutton"));
    ui->max_mtime_button = GTK_BUTTON(gtk_builder_get_object(builder, "max_mtime_button"));

    ui->path_list_store = GTK_LIST_STORE(gtk_builder_get_object(builder, "path_list_store"));

    ui->date_dlg = GTK_DIALOG(gtk_builder_get_object(builder, "date_dlg"));
    gtk_dialog_set_alternative_button_order(GTK_DIALOG(ui->date_dlg), GTK_RESPONSE_OK, GTK_RESPONSE_CANCEL);
    ui->calendar = GTK_CALENDAR(gtk_builder_get_object(builder, "calendar"));

    filesearch_glade_connect_signals(builder, ui);
    g_object_unref(builder);

    /* associate the data with the dialog so it can be freed as needed. */
    g_object_set_qdata_full(ui->dlg, fm_qdata_id, ui, (GDestroyNotify)file_search_ui_free);

    /* add folders to search */
    if(folder_paths)
    {
        GList* l;
        for(l = folder_paths; l; l=l->next)
        {
            FmPath* folder_path = FM_PATH(l->data);
            char* path_str = fm_path_to_str(folder_path);
            add_path(ui->path_list_store, path_str);
            g_free(path_str);
        }
    }

    gtk_widget_show(ui->dlg);

    pcmanfm_ref();

    return TRUE;
}

static void on_bigger_than_checkbutton_toggled(GtkToggleButton* btn, gpointer user_data)
{
    FileSearchUI* ui = (FileSearchUI*)user_data;
    gboolean enable = gtk_toggle_button_get_active(btn);
    gtk_widget_set_sensitive(GTK_WIDGET(ui->bigger_spinbutton), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(ui->bigger_unit_combo), enable);
}

static void on_smaller_than_checkbutton_toggled(GtkToggleButton* btn, gpointer user_data)
{
    FileSearchUI* ui = (FileSearchUI*)user_data;
    gboolean enable = gtk_toggle_button_get_active(btn);
    gtk_widget_set_sensitive(GTK_WIDGET(ui->smaller_spinbutton), enable);
    gtk_widget_set_sensitive(GTK_WIDGET(ui->smaller_unit_combo), enable);
}

static gboolean choose_date(GtkButton* btn, FileSearchUI* ui)
{
    const char* label = gtk_button_get_label(btn);
    int res;
    guint month, year, day;

    /* FIXME: we definitely need a better UI design for this part. */

    if(sscanf(label, "%04d-%02d-%02d", &year, &month, &day) == 3)
    {
        gtk_calendar_select_month(ui->calendar, month, year);
        gtk_calendar_select_day(ui->calendar, day);
    }

    res = gtk_dialog_run(ui->date_dlg);
    gtk_widget_hide(GTK_WIDGET(ui->date_dlg));

    if(res == GTK_RESPONSE_OK)
    {
        char str[12];
        gtk_calendar_get_date(ui->calendar, &year, &month, &day);
        ++month; /* month returned from GtkCalendar is 0 based. */
        g_snprintf(str, sizeof(str), "%04d-%02d-%02d", year, month, day);
        gtk_button_set_label(btn, str);        
        return TRUE;
    }
    return FALSE;
}

static void on_min_mtime_button_clicked(GtkButton* btn, gpointer user_data)
{
    FileSearchUI* ui = (FileSearchUI*)user_data;
    choose_date(btn, ui);
}

static void on_max_mtime_button_clicked(GtkButton* btn, gpointer user_data)
{
    FileSearchUI* ui = (FileSearchUI*)user_data;
    choose_date(btn, ui);
}

static void on_min_mtime_checkbutton_toggled(GtkToggleButton* btn, gpointer user_data)
{
    FileSearchUI* ui = (FileSearchUI*)user_data;
    gboolean enable = gtk_toggle_button_get_active(btn);
    gtk_widget_set_sensitive(GTK_WIDGET(ui->min_mtime_button), enable);
}

static void on_max_mtime_checkbutton_toggled(GtkToggleButton* btn, gpointer user_data)
{
    FileSearchUI* ui = (FileSearchUI*)user_data;
    gboolean enable = gtk_toggle_button_get_active(btn);
    gtk_widget_set_sensitive(GTK_WIDGET(ui->max_mtime_button), enable);
}

#include "file-search-ui.c" /* file generated with glade-connect-gen */

#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// 初始化GUI
int gui_init(int argc, char* argv[]) {
    // 初始化GTK
    gtk_init(&argc, &argv);
    
    // 创建主窗口
    g_gui.main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_gui.main_window), "Base64网络传输客户端");
    gtk_window_set_default_size(GTK_WINDOW(g_gui.main_window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(g_gui.main_window), GTK_WIN_POS_CENTER);
    
    // 连接窗口关闭信号
    g_signal_connect(g_gui.main_window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    // 创建主容器
    GtkWidget* main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(g_gui.main_window), main_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 10);
    
    // 创建连接区域
    GtkWidget* connection_frame = gtk_frame_new("服务器连接");
    gtk_box_pack_start(GTK_BOX(main_vbox), connection_frame, FALSE, FALSE, 0);
    
    GtkWidget* connection_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(connection_frame), connection_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(connection_vbox), 10);
    
    // 服务器地址输入
    GtkWidget* server_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(connection_vbox), server_hbox, FALSE, FALSE, 0);
    
    GtkWidget* server_label = gtk_label_new("服务器地址:");
    gtk_box_pack_start(GTK_BOX(server_hbox), server_label, FALSE, FALSE, 0);
    
    g_gui.server_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(g_gui.server_entry), g_client.config.server_host);
    gtk_box_pack_start(GTK_BOX(server_hbox), g_gui.server_entry, TRUE, TRUE, 0);
    
    GtkWidget* port_label = gtk_label_new("端口:");
    gtk_box_pack_start(GTK_BOX(server_hbox), port_label, FALSE, FALSE, 0);
    
    g_gui.port_entry = gtk_entry_new();
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", g_client.config.server_port);
    gtk_entry_set_text(GTK_ENTRY(g_gui.port_entry), port_str);
    gtk_entry_set_width_chars(GTK_ENTRY(g_gui.port_entry), 8);
    gtk_box_pack_start(GTK_BOX(server_hbox), g_gui.port_entry, FALSE, FALSE, 0);
    
    // 连接按钮
    GtkWidget* button_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(connection_vbox), button_hbox, FALSE, FALSE, 0);
    
    g_gui.connect_button = gtk_button_new_with_label("连接");
    g_signal_connect(g_gui.connect_button, "clicked", G_CALLBACK(on_connect_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(button_hbox), g_gui.connect_button, FALSE, FALSE, 0);
    
    g_gui.disconnect_button = gtk_button_new_with_label("断开");
    g_signal_connect(g_gui.disconnect_button, "clicked", G_CALLBACK(on_disconnect_clicked), NULL);
    gtk_widget_set_sensitive(g_gui.disconnect_button, FALSE);
    gtk_box_pack_start(GTK_BOX(button_hbox), g_gui.disconnect_button, FALSE, FALSE, 0);
    
    // 状态标签
    g_gui.status_label = gtk_label_new("状态: 未连接");
    gtk_box_pack_start(GTK_BOX(button_hbox), g_gui.status_label, TRUE, TRUE, 0);
    gtk_label_set_xalign(GTK_LABEL(g_gui.status_label), 0.0);
    
    // 创建文件上传区域
    GtkWidget* file_frame = gtk_frame_new("文件上传");
    gtk_box_pack_start(GTK_BOX(main_vbox), file_frame, FALSE, FALSE, 0);
    
    GtkWidget* file_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(file_frame), file_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(file_vbox), 10);
    
    GtkWidget* file_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(file_vbox), file_hbox, FALSE, FALSE, 0);
    
    g_gui.file_chooser_button = gtk_file_chooser_button_new("选择文件", GTK_FILE_CHOOSER_ACTION_OPEN);
    g_signal_connect(g_gui.file_chooser_button, "file-set", G_CALLBACK(on_file_chosen), NULL);
    gtk_box_pack_start(GTK_BOX(file_hbox), g_gui.file_chooser_button, TRUE, TRUE, 0);
    
    g_gui.upload_button = gtk_button_new_with_label("上传文件");
    g_signal_connect(g_gui.upload_button, "clicked", G_CALLBACK(on_file_upload_clicked), NULL);
    gtk_widget_set_sensitive(g_gui.upload_button, FALSE);
    gtk_box_pack_start(GTK_BOX(file_hbox), g_gui.upload_button, FALSE, FALSE, 0);
    
    // 进度条
    g_gui.progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(file_vbox), g_gui.progress_bar, FALSE, FALSE, 0);
    
    // 创建数据上传区域
    GtkWidget* data_frame = gtk_frame_new("数据上传");
    gtk_box_pack_start(GTK_BOX(main_vbox), data_frame, FALSE, FALSE, 0);
    
    GtkWidget* data_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(data_frame), data_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(data_vbox), 10);
    
    GtkWidget* data_hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(data_vbox), data_hbox1, FALSE, FALSE, 0);
    
    GtkWidget* table_label = gtk_label_new("表名:");
    gtk_box_pack_start(GTK_BOX(data_hbox1), table_label, FALSE, FALSE, 0);
    
    g_gui.table_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_gui.table_entry), "输入表名");
    gtk_box_pack_start(GTK_BOX(data_hbox1), g_gui.table_entry, TRUE, TRUE, 0);
    
    GtkWidget* field_label = gtk_label_new("字段名:");
    gtk_box_pack_start(GTK_BOX(data_hbox1), field_label, FALSE, FALSE, 0);
    
    g_gui.field_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_gui.field_entry), "输入字段名");
    gtk_box_pack_start(GTK_BOX(data_hbox1), g_gui.field_entry, TRUE, TRUE, 0);
    
    GtkWidget* data_hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(data_vbox), data_hbox2, FALSE, FALSE, 0);
    
    GtkWidget* data_label = gtk_label_new("数据:");
    gtk_box_pack_start(GTK_BOX(data_hbox2), data_label, FALSE, FALSE, 0);
    
    g_gui.data_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_gui.data_entry), "输入要上传的数据");
    gtk_box_pack_start(GTK_BOX(data_hbox2), g_gui.data_entry, TRUE, TRUE, 0);
    
    g_gui.data_upload_button = gtk_button_new_with_label("上传数据");
    g_signal_connect(g_gui.data_upload_button, "clicked", G_CALLBACK(on_data_upload_clicked), NULL);
    gtk_widget_set_sensitive(g_gui.data_upload_button, FALSE);
    gtk_box_pack_start(GTK_BOX(data_hbox2), g_gui.data_upload_button, FALSE, FALSE, 0);
    
    // 创建日志区域
    GtkWidget* log_frame = gtk_frame_new("操作日志");
    gtk_box_pack_start(GTK_BOX(main_vbox), log_frame, TRUE, TRUE, 0);
    
    GtkWidget* log_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(log_frame), log_scrolled);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_scrolled), 
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    g_gui.log_textview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(g_gui.log_textview), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(g_gui.log_textview), FALSE);
    gtk_container_add(GTK_CONTAINER(log_scrolled), g_gui.log_textview);
    
    g_gui.log_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_gui.log_textview));
    
    // 显示所有组件
    gtk_widget_show_all(g_gui.main_window);
    
    // 初始化日志
    gui_log_message("客户端启动完成");
    
    printf("GUI初始化完成\n");
    return 0;
}

// 清理GUI资源
void gui_cleanup() {
    // GTK会自动清理资源
}

// 运行GUI主循环
void gui_run() {
    gtk_main();
}

// 更新状态显示
void gui_update_status(const char* status) {
    if (!status || !g_gui.status_label) return;
    
    char status_text[256];
    snprintf(status_text, sizeof(status_text), "状态: %s", status);
    gtk_label_set_text(GTK_LABEL(g_gui.status_label), status_text);
    
    // 根据连接状态启用/禁用按钮
    gboolean connected = is_connected();
    gtk_widget_set_sensitive(g_gui.connect_button, !connected);
    gtk_widget_set_sensitive(g_gui.disconnect_button, connected);
    gtk_widget_set_sensitive(g_gui.upload_button, connected);
    gtk_widget_set_sensitive(g_gui.data_upload_button, connected);
}

// 记录日志消息
void gui_log_message(const char* format, ...) {
    if (!format || !g_gui.log_buffer) return;
    
    // 格式化用户消息
    va_list args;
    va_start(args, format);
    char user_message[512];
    vsnprintf(user_message, sizeof(user_message), format, args);
    va_end(args);
    
    // 获取当前时间
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", timeinfo);
    
    // 格式化最终消息
    char formatted_message[1024];
    snprintf(formatted_message, sizeof(formatted_message), "[%s] %s\n", timestamp, user_message);
    
    // 添加到文本缓冲区
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(g_gui.log_buffer, &iter);
    gtk_text_buffer_insert(g_gui.log_buffer, &iter, formatted_message, -1);
    
    // 滚动到底部
    GtkTextMark* mark = gtk_text_buffer_get_insert(g_gui.log_buffer);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(g_gui.log_textview), mark);
}

// 设置进度条
void gui_set_progress(double progress) {
    if (!g_gui.progress_bar) return;
    
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(g_gui.progress_bar), progress);
    
    if (progress > 0.0 && progress < 1.0) {
        char progress_text[64];
        snprintf(progress_text, sizeof(progress_text), "%.1f%%", progress * 100);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(g_gui.progress_bar), progress_text);
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(g_gui.progress_bar), TRUE);
    } else {
        gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(g_gui.progress_bar), FALSE);
    }
}

// 显示错误对话框
void gui_show_error(const char* message) {
    if (!message) return;
    
    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(g_gui.main_window),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_ERROR,
                                              GTK_BUTTONS_OK,
                                              "%s", message);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// 显示信息对话框
void gui_show_info(const char* message) {
    if (!message) return;
    
    GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(g_gui.main_window),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_INFO,
                                              GTK_BUTTONS_OK,
                                              "%s", message);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// 格式化日志消息
void log_message_to_gui(const char* format, ...) {
    if (!format) return;
    
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    gui_log_message(message);
    printf("%s\n", message); // 同时输出到控制台
}

// GUI回调函数
void on_connect_clicked(GtkWidget* widget, gpointer data) {
    (void)widget; (void)data; // 避免未使用参数警告
    
    const char* host = gtk_entry_get_text(GTK_ENTRY(g_gui.server_entry));
    const char* port_str = gtk_entry_get_text(GTK_ENTRY(g_gui.port_entry));
    
    if (!host || strlen(host) == 0) {
        gui_show_error("请输入服务器地址");
        return;
    }
    
    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
        gui_show_error("请输入有效的端口号 (1-65535)");
        return;
    }
    
    gui_log_message("正在连接到服务器...");
    gui_update_status("连接中");
    
    if (client_connect(host, port) == 0) {
        gui_log_message("连接成功");
        gui_update_status("已连接");
    } else {
        gui_log_message("连接失败");
        gui_update_status("连接失败");
        gui_show_error("无法连接到服务器");
    }
}

void on_disconnect_clicked(GtkWidget* widget, gpointer data) {
    (void)widget; (void)data; // 避免未使用参数警告
    
    client_disconnect();
    gui_log_message("已断开连接");
    gui_update_status("已断开");
}

void on_file_upload_clicked(GtkWidget* widget, gpointer data) {
    (void)widget; (void)data; // 避免未使用参数警告
    
    char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(g_gui.file_chooser_button));
    
    if (!filename) {
        gui_show_error("请先选择要上传的文件");
        return;
    }
    
    if (!is_connected()) {
        gui_show_error("请先连接到服务器");
        g_free(filename);
        return;
    }
    
    gui_log_message("开始上传文件: %s", filename);
    gui_set_progress(0.1);
    
    if (upload_file(filename) == 0) {
        gui_log_message("文件上传成功");
        gui_set_progress(1.0);
    } else {
        gui_log_message("文件上传失败");
        gui_set_progress(0.0);
        gui_show_error("文件上传失败");
    }
    
    g_free(filename);
}

void on_data_upload_clicked(GtkWidget* widget, gpointer data) {
    (void)widget; (void)data; // 避免未使用参数警告
    
    const char* table_name = gtk_entry_get_text(GTK_ENTRY(g_gui.table_entry));
    const char* field_name = gtk_entry_get_text(GTK_ENTRY(g_gui.field_entry));
    const char* data_value = gtk_entry_get_text(GTK_ENTRY(g_gui.data_entry));
    
    if (!table_name || strlen(table_name) == 0) {
        gui_show_error("请输入表名");
        return;
    }
    
    if (!field_name || strlen(field_name) == 0) {
        gui_show_error("请输入字段名");
        return;
    }
    
    if (!data_value || strlen(data_value) == 0) {
        gui_show_error("请输入要上传的数据");
        return;
    }
    
    if (!is_connected()) {
        gui_show_error("请先连接到服务器");
        return;
    }
    
    gui_log_message("开始上传数据: %s.%s", table_name, field_name);
    
    if (send_data_upload(table_name, field_name, data_value) == 0) {
        gui_log_message("数据上传成功");
        // 清空输入框
        gtk_entry_set_text(GTK_ENTRY(g_gui.table_entry), "");
        gtk_entry_set_text(GTK_ENTRY(g_gui.field_entry), "");
        gtk_entry_set_text(GTK_ENTRY(g_gui.data_entry), "");
    } else {
        gui_log_message("数据上传失败");
        gui_show_error("数据上传失败");
    }
}

void on_window_destroy(GtkWidget* widget, gpointer data) {
    (void)widget; (void)data; // 避免未使用参数警告
    
    g_client.running = 0;
    client_disconnect();
    gtk_main_quit();
}

void on_file_chosen(GtkFileChooserButton* widget, gpointer data) {
    (void)widget; (void)data; // 避免未使用参数警告
    
    char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
    if (filename) {
        gui_log_message("选择文件: %s", filename);
        g_free(filename);
    }
}
/*
 * command mode options (:set var=val)
 *
 * Copyright 2005 Timo Hirvonen
 */

#include <options.h>
#include <command_mode.h>
#include <ui_curses.h>
#include <format_print.h>
#include <player.h>
#include <buffer.h>
#include <sconf.h>
#include <misc.h>
#include <xmalloc.h>
#include <utils.h>
#include <debug.h>

/*
 * opt->data is color index
 *         0..NR_COLORS-1    is bg
 * NR_COLORS..2*NR_COLORS-1  is fg
 */
static void get_color(const struct command_mode_option *opt, char **value)
{
	int i = (int)opt->data;
	int color;
	char buf[32];

	BUG_ON(i < 0);
	BUG_ON(i >= 2 * NR_COLORS);

	if (i < NR_COLORS) {
		color = bg_colors[i];
	} else {
		color = fg_colors[i - NR_COLORS];
	}
	snprintf(buf, sizeof(buf), "%d", color);
	*value = xstrdup(buf);
}

static void set_color(const struct command_mode_option *opt, const char *value)
{
	int i = (int)opt->data;
	int color_max = 255;
	long int color;

	BUG_ON(i < 0);
	BUG_ON(i >= 2 * NR_COLORS);

	if (str_to_int(value, &color) || color < -1 || color > color_max) {
		ui_curses_display_error_msg("color value must be -1..%d", color_max);
		return;
	}

	if (i < NR_COLORS) {
		bg_colors[i] = color;
	} else {
		i -= NR_COLORS;
		fg_colors[i] = color;
	}
	ui_curses_update_color(i);
}

static void get_format(const struct command_mode_option *opt, char **value)
{
	char **var = opt->data;

	*value = xstrdup(*var);
}

static void set_format(const struct command_mode_option *opt, const char *value)
{
	char **var = opt->data;

	d_print("%s=%s (old=%s)\n", opt->name, value, *var);
	if (!format_valid(value)) {
		ui_curses_display_error_msg("invalid format string");
		return;
	}
	free(*var);
	*var = xstrdup(value);
	ui_curses_update_view();
	ui_curses_update_titleline();
}

static void get_output_plugin(const struct command_mode_option *opt, char **value)
{
	*value = player_get_op();
	if (*value == NULL)
		*value = xstrdup("");
}

static void set_output_plugin(const struct command_mode_option *opt, const char *value)
{
	player_set_op(value);
}

static void get_buffer_seconds(const struct command_mode_option *opt, char **value)
{
	char buf[32];

	snprintf(buf, sizeof(buf), "%d", player_get_buffer_size() * CHUNK_SIZE / (44100 * 16 / 8 * 2));
	*value = xstrdup(buf);
}

static void set_buffer_seconds(const struct command_mode_option *opt, const char *value)
{
	long int seconds;

	if (str_to_int(value, &seconds) == -1) {
		ui_curses_display_error_msg("invalid format string");
		return;
	}
	player_set_buffer_size(seconds * 44100 * 16 / 8 * 2 / CHUNK_SIZE);
}

static void get_status_display_program(const struct command_mode_option *opt, char **value)
{
	if (status_display_program == NULL) {
		*value = xstrdup("");
	} else {
		*value = xstrdup(status_display_program);
	}
}

static void set_status_display_program(const struct command_mode_option *opt, const char *value)
{
	free(status_display_program);
	if (strcmp(value, "") == 0) {
		status_display_program = NULL;
		return;
	}
	status_display_program = xstrdup(value);
}

static void get_sort(const struct command_mode_option *opt, char **value)
{
	*value = xstrdup(sort_string);
}

static void set_sort(const struct command_mode_option *opt, const char *value)
{
	ui_curses_set_sort(value, 1);
	ui_curses_update_view();
}

static void get_op_option(const struct command_mode_option *opt, char **value)
{
	*value = xstrdup("");
}

static void set_op_option(const struct command_mode_option *opt, const char *value)
{
	d_print("%s=%s\n", opt->name, value);
	BUG_ON(opt->data != NULL);
	player_set_op_option(opt->name, value);
}

static void player_option_callback(void *data, const char *name)
{
	d_print("adding player option %s\n", name);
	option_add(name, get_op_option, set_op_option, data);
}

#define NR_FMTS 8
static const char *fmt_names[NR_FMTS] = {
	"altformat_current",
	"altformat_playlist",
	"altformat_title",
	"altformat_trackwin",
	"format_current",
	"format_playlist",
	"format_title",
	"format_trackwin"
};

static const char *fmt_defaults[NR_FMTS] = {
	" %F%= %d ",
	" %f%= %d ",
	"%f",
	" %f%= %d ",
	" %a - %l - %02n. %t%= %y %d ",
	" %a - %l - %02n. %t%= %y %d ",
	"%a - %l - %t (%y)",
	" %02n. %t%= %y %d "
};

static char **fmt_vars[NR_FMTS] = {
	&current_alt_format,
	&list_win_alt_format,
	&window_title_alt_format,
	&track_win_alt_format,
	&current_format,
	&list_win_format,
	&window_title_format,
	&track_win_format
};

void options_init(void)
{
	int i;

	for (i = 0; i < NR_FMTS; i++) {
		char *var;

		if (sconf_get_str_option(&sconf_head, fmt_names[i], &var))
			var = xstrdup(fmt_defaults[i]);
		*fmt_vars[i] = var;
		option_add(fmt_names[i], get_format, set_format, fmt_vars[i]);
	}

	for (i = 0; i < NR_COLORS; i++) {
		char buf[64];

		snprintf(buf, sizeof(buf), "color_%s_bg", color_names[i]);
		option_add(buf, get_color, set_color, (void *)i);
		snprintf(buf, sizeof(buf), "color_%s_fg", color_names[i]);
		option_add(buf, get_color, set_color, (void *)(i + NR_COLORS));
	}

	option_add("output_plugin", get_output_plugin, set_output_plugin, NULL);
	option_add("buffer_seconds", get_buffer_seconds, set_buffer_seconds, NULL);
	option_add("status_display_program", get_status_display_program, set_status_display_program, NULL);
	option_add("sort", get_sort, set_sort, NULL);

	player_for_each_op_option(player_option_callback, NULL);
}

void options_exit(void)
{
	int i;

	for (i = 0; i < NR_FMTS; i++)
		sconf_set_str_option(&sconf_head, fmt_names[i], *fmt_vars[i]);
}
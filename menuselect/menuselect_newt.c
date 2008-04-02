/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2008 Sean Bright
 *
 * Sean Bright <sean.bright@gmail.com>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*
 * \file
 *
 * \author Sean Bright <sean.bright@gmail.com>
 * 
 * \brief newt frontend for selection maintenance
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <newt.h>

#include "menuselect.h"

#define MIN_X 80
#define MIN_Y 21

extern int changes_made;

static newtComponent rootOptions;
static newtComponent subOptions;

static newtComponent memberNameTextbox;
static newtComponent dependsLabel;
static newtComponent usesLabel;
static newtComponent conflictsLabel;
static newtComponent dependsDataTextbox;
static newtComponent usesDataTextbox;
static newtComponent conflictsDataTextbox;

static void root_menu_callback(newtComponent component, void *data);


static void toggle_all_options(int select)
{
	struct category *cat = newtListboxGetCurrent(rootOptions);
	struct member *mem = newtListboxGetCurrent(subOptions);

	set_all(cat, select);

	// Redraw
	root_menu_callback(rootOptions, NULL);

	// Set our selection back
	newtListboxSetCurrentByKey(subOptions, mem);

	return;
}

static void toggle_selected_option()
{
	struct member *mem = newtListboxGetCurrent(subOptions);

	toggle_enabled(mem);

	// Redraw the menu
	root_menu_callback(rootOptions, NULL);

	// Set our selection back to what it should be
	newtListboxSetCurrentByKey(subOptions, mem);

	return;
}

static void reset_display()
{
	newtTextboxSetText(memberNameTextbox, "");
	newtTextboxSetText(dependsDataTextbox, "");
	newtTextboxSetText(usesDataTextbox, "");
	newtTextboxSetText(conflictsDataTextbox, "");
	newtRefresh();
}

static void display_member_info(struct member *mem)
{
	char buffer[128] = { 0 };

	struct depend *dep;
	struct conflict *con;
	struct use *uses;

	reset_display();

	if (mem->displayname) {
		newtTextboxSetText(memberNameTextbox, mem->displayname);
	}

	if (AST_LIST_EMPTY(&mem->deps)) {
		newtTextboxSetText(dependsDataTextbox, "N/A");
	} else {
		strcpy(buffer, "");
		AST_LIST_TRAVERSE(&mem->deps, dep, list) {
			strncat(buffer, dep->name, sizeof(buffer) - strlen(buffer) - 1);
			strncat(buffer, dep->member ? "(M)" : "(E)", sizeof(buffer) - strlen(buffer) - 1);
			if (AST_LIST_NEXT(dep, list))
				strncat(buffer, ", ", sizeof(buffer) - strlen(buffer) - 1);
		}
		newtTextboxSetText(dependsDataTextbox, buffer);
	}

	if (AST_LIST_EMPTY(&mem->uses)) {
		newtTextboxSetText(usesDataTextbox, "N/A");
	} else {
		strcpy(buffer, "");
		AST_LIST_TRAVERSE(&mem->uses, uses, list) {
			strncat(buffer, uses->name, sizeof(buffer) - strlen(buffer) - 1);
			if (AST_LIST_NEXT(uses, list))
				strncat(buffer, ", ", sizeof(buffer) - strlen(buffer) - 1);
		}
		newtTextboxSetText(usesDataTextbox, buffer);
	}

	if (AST_LIST_EMPTY(&mem->conflicts)) {
		newtTextboxSetText(conflictsDataTextbox, "N/A");
	} else {
		strcpy(buffer, "");
		AST_LIST_TRAVERSE(&mem->conflicts, con, list) {
			strncat(buffer, con->name, sizeof(buffer) - strlen(buffer) - 1);
			strncat(buffer, con->member ? "(M)" : "(E)", sizeof(buffer) - strlen(buffer) - 1);
			if (AST_LIST_NEXT(con, list))
				strncat(buffer, ", ", sizeof(buffer) - strlen(buffer) - 1);
		}
		newtTextboxSetText(conflictsDataTextbox, buffer);
	}

	return;
}

static void build_members_menu()
{
	struct category *cat;
	struct member *mem;
	char buf[64];

	reset_display();

	newtListboxClear(subOptions);

	cat = newtListboxGetCurrent(rootOptions);

	AST_LIST_TRAVERSE(&cat->members, mem, list) {

		if ((mem->depsfailed == HARD_FAILURE) || (mem->conflictsfailed == HARD_FAILURE)) {
			snprintf(buf, sizeof(buf), "XXX %s", mem->name);
		} else if (mem->depsfailed == SOFT_FAILURE) {
			snprintf(buf, sizeof(buf), "<%s> %s", mem->enabled ? "*" : " ", mem->name);
		} else if (mem->conflictsfailed == SOFT_FAILURE) {
			snprintf(buf, sizeof(buf), "(%s) %s", mem->enabled ? "*" : " ", mem->name);
		} else {
			snprintf(buf, sizeof(buf), "[%s] %s", mem->enabled ? "*" : " ", mem->name);
		}

		newtListboxAppendEntry(subOptions, buf, mem);
	}

	display_member_info(AST_LIST_FIRST(&cat->members));

	return;
}

static void build_main_menu()
{
	struct category *cat;
	char buf[64];
	int i = 1;

	newtListboxClear(rootOptions);

	AST_LIST_TRAVERSE(&categories, cat, list) {
		if (!strlen_zero(cat->displayname))
			snprintf(buf, sizeof(buf), " %s ", cat->displayname);
		else
			snprintf(buf, sizeof(buf), " %s ", cat->name);

		newtListboxAppendEntry(rootOptions, buf, cat);

		i++;
	}
}

static void category_menu_callback(newtComponent component, void *data)
{
	display_member_info(newtListboxGetCurrent(subOptions));
}

static void root_menu_callback(newtComponent component, void *data)
{
	build_members_menu();
}

int run_confirmation_dialog(int *result)
{
	int res = newtWinTernary("Are You Sure?", "Discard changes & Exit", "Save & Exit", "Cancel",
				   "It appears you have made some changes, and you have opted to Quit "
				   "without saving these changes.  Please choose \"Discard changes & Exit\" to exit "
				   "without saving; Choose \"Cancel\" to cancel your decision to quit, and keep "
				   "working in menuselect, or choose \"Save & Exit\" to save your changes, and exit.");

	switch (res) {
	case 1:
		/* Discard and exit */
		*result = -1;
		return 1;
	case 2:
		/* Save and exit */
		*result = 0;
		return 1;
	case 3:
		/* They either chose "No" or they hit F12 */
	default:
		*result = -1;
		return 0;
	}
}

int run_menu(void)
{
	struct newtExitStruct es;
	newtComponent form;
	int x = 0, y = 0, res = 0;

	newtInit();
	newtCls();
	newtGetScreenSize(&x, &y);

	if (x < MIN_X || y < MIN_Y) {
		newtFinished();
		fprintf(stderr, "Terminal must be at least %d x %d.\n", MIN_X, MIN_Y);
		return -1;
	}

	newtPushHelpLine("  <ENTER> toggles selection | <F12> saves & exits | <ESC> exits without save");
	newtRefresh();

	newtCenteredWindow(x - 8, y - 7, menu_name);
	form = newtForm(NULL, NULL, 0);

	/* F8 for select all */
	newtFormAddHotKey(form, NEWT_KEY_F8);

	/* F7 for deselect all */
	newtFormAddHotKey(form, NEWT_KEY_F7);

	newtFormSetTimer(form, 200);

	rootOptions = newtListbox(2, 1, y - 16, 0);
	newtListboxSetWidth(rootOptions, 29);
	newtFormAddComponent(form, rootOptions);
	newtComponentAddCallback(rootOptions, root_menu_callback, NULL);

	subOptions = newtListbox(33, 1, y - 16, NEWT_FLAG_SCROLL | NEWT_FLAG_RETURNEXIT);
	newtListboxSetWidth(subOptions, x - 42);
	newtFormAddComponent(form, subOptions);
	newtComponentAddCallback(subOptions, category_menu_callback, NULL);

	memberNameTextbox    = newtTextbox(2, y - 13, x - 10, 1, 0);
	dependsLabel         = newtLabel(2, y - 11, "    Depends on:");
	usesLabel            = newtLabel(2, y - 10, "       Can use:");
	conflictsLabel       = newtLabel(2, y - 9, "Conflicts with:");
	dependsDataTextbox   = newtTextbox(18, y - 11, x - 27, 1, 0);
	usesDataTextbox      = newtTextbox(18, y - 10, x - 27, 1, 0);
	conflictsDataTextbox = newtTextbox(18, y - 9, x - 27, 1, 0);

	newtFormAddComponents(
		form,
		memberNameTextbox,
		dependsLabel,
		dependsDataTextbox,
		usesLabel,
		usesDataTextbox,
		conflictsLabel,
		conflictsDataTextbox,
		NULL);

	build_main_menu();

	root_menu_callback(rootOptions, AST_LIST_FIRST(&categories));

	for (;;) {
		do {
			newtFormRun(form, &es);
		} while (es.reason == NEWT_EXIT_TIMER);

		if (es.reason == NEWT_EXIT_HOTKEY) {
			int done = 1;

			switch (es.u.key) {
			case NEWT_KEY_F12:
				res = 0;
				break;
			case NEWT_KEY_F7:
				toggle_all_options(0);
				done = 0;
				break;
			case NEWT_KEY_F8:
				toggle_all_options(1);
				done = 0;
				break;
			case NEWT_KEY_ESCAPE:
			case NEWT_KEY_F10:
				if (changes_made) {
					done = run_confirmation_dialog(&res);
				} else {
					res = -1;
				}
				break;
			default:
				done = 0;
				break;
			}

			if (done)
				break;
		} else if (es.reason == NEWT_EXIT_COMPONENT) {
			toggle_selected_option();
		}
	}

	/* Cleanup */
	newtFormDestroy(form);
	newtPopWindow();
	newtCls();
	newtFinished();

	return res;
}

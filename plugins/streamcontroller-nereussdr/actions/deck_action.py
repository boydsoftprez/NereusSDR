# SPDX-License-Identifier: GPL-3.0-or-later
# Modification history (NereusSDR):
#   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
#                AI-assisted implementation via Anthropic Claude Code.
#   2026-11-06 - J.J. Boyd (KG4VCF), a key that leaves the deck while down lets go.
#                AI-assisted implementation via Anthropic Claude Code.
"""One NereusSDR action on a key or dial. Host calls and events as AetherSDR
plugins/streamcontroller-aethersdr/actions/PTT/PTT.py and actions/TX/RFPower/RFPower.py
[@1e0718ad]. Those are the only host calls used: labels have no source here, so the
display line isn't shown on keys.

What a held key can leave by, from StreamController src/backend at 0f439967a16a [main]:
  Removed from the page: PluginManager/ActionCore.py:679 on_removed_from_cache, called from
    PageManagement/Page.py:171,288,582 and windows/.../ActionManager.py:645. The one real hook.
  Another page: DeckController.py:1352 leaves clear_action_objects off, so the old page's action
    gets no call, and the release goes to whatever the new page has on that key.
  The deck going away: DeckController.py:1815-1818 delete() empties the page's actions with no call.
  The screen saver: DeckController.py:3242-3247 eats a release while it shows.
Those three have no call, and ActionCore.py:446 get_is_present plus DeckController.py:1837 get_alive
are what can see them, so main.py looks at each held action with let_go_of_what_left. The plugin
runs in the host's process, so the socket stays open and the window sees no drop.
"""
import os
import threading

from src.backend.PluginManager.ActionBase import ActionBase
from src.backend.DeckManagement.InputIdentifier import Input

import gi
gi.require_version("Gtk", "4.0")
gi.require_version("Adw", "1")
from gi.repository import Adw, GLib, Gtk

SLICES = [
    ("The slice the window is on", ""),
    ("Slice A", "A"),
    ("Slice B", "B"),
    ("Slice C", "C"),
    ("Slice D", "D"),
]


def let_go_of_what_left(held):
    for action in list(held):
        if not action.still_on_deck():
            action.let_go()


class DeckAction(ActionBase):
    ACTION_ID = ""      # set on each class main.py makes
    # The host's reader thread and main.py's check both change _down. Held across the
    # send so a press and its release can't go out in the wrong order.
    _down_lock = threading.Lock()

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._down = False

    def on_ready(self):
        self.plugin_base.deck_actions.add(self)
        self.show_light()

    def show_light(self):
        # The link calls this from its own thread.
        name = "on.png" if self.ACTION_ID in self.plugin_base.lit else "off.png"
        path = os.path.join(self.plugin_base.PATH, "assets", name)
        GLib.idle_add(lambda: self.set_media(media_path=path) and False)

    def _pin(self):
        return (self.get_settings() or {}).get("slice") or None

    def let_go(self):
        with DeckAction._down_lock:
            if self._down:
                self._down = False
                self.plugin_base.held.discard(self)
                self.plugin_base.link.key_up(self.ACTION_ID)

    def still_on_deck(self):
        try:
            return self.deck_controller.get_alive() and self.get_is_present()
        except Exception:   # a deck that is going away can raise anything
            return False

    def on_removed_from_cache(self):
        self.let_go()
        super().on_removed_from_cache()

    def event_callback(self, event, data):
        link = self.plugin_base.link
        if event == Input.Key.Events.DOWN:
            with DeckAction._down_lock:
                self._down = True
                self.plugin_base.held.add(self)
                link.key_down(self.ACTION_ID, self._pin())
        elif event == Input.Key.Events.UP:
            self.let_go()
        elif event == Input.Dial.Events.TURN_CW:
            link.dial_ticks(self.ACTION_ID, 1, self._pin())
        elif event == Input.Dial.Events.TURN_CCW:
            link.dial_ticks(self.ACTION_ID, -1, self._pin())

    def get_config_rows(self):
        rows = super().get_config_rows()
        model = Gtk.StringList()
        for label, _ in SLICES:
            model.append(label)
        self._slice_row = Adw.ComboRow(model=model, title="Slice")
        saved = (self.get_settings() or {}).get("slice", "")
        for index, (_, letter) in enumerate(SLICES):
            if letter == saved:
                self._slice_row.set_selected(index)
        self._slice_row.connect("notify::selected", self._slice_changed)
        rows.append(self._slice_row)
        return rows

    def _slice_changed(self, row, *_):
        settings = self.get_settings() or {}
        settings["slice"] = SLICES[row.get_selected()][1]
        self.set_settings(settings)

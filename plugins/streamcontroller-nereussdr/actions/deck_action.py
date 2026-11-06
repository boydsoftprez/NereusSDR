# SPDX-License-Identifier: GPL-3.0-or-later
# Modification history (NereusSDR):
#   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
#                AI-assisted implementation via Anthropic Claude Code.
"""One NereusSDR action on a key or dial. Host calls and events as AetherSDR
plugins/streamcontroller-aethersdr/actions/PTT/PTT.py and actions/TX/RFPower/RFPower.py
[@1e0718ad]. Those are the only host calls used: labels have no source here, so the
display line isn't shown on keys.
"""
import os

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


class DeckAction(ActionBase):
    ACTION_ID = ""      # set on each class main.py makes

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

    def event_callback(self, event, data):
        link = self.plugin_base.link
        if event == Input.Key.Events.DOWN:
            self._down = True
            link.key_down(self.ACTION_ID, self._pin())
        elif event == Input.Key.Events.UP:
            self._down = False
            link.key_up(self.ACTION_ID)
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

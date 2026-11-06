# SPDX-License-Identifier: GPL-3.0-or-later
# Modification history (NereusSDR):
#   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
#                AI-assisted implementation via Anthropic Claude Code.
"""NereusSDR for StreamController, in the app's process. Linux only. Written for NereusSDR
after reading how the AetherSDR plugin registers with the host; none of its code is here, and
this plugin calls no TCI. Registers one action per entry in actions/actions.json.

Host classes as AetherSDR plugins/streamcontroller-aethersdr/main.py:18-21,98-106,175-180 [@1e0718ad].
"""
import json
import os
import weakref

from src.backend.PluginManager.PluginBase import PluginBase
from src.backend.PluginManager.ActionHolder import ActionHolder
from src.backend.DeckManagement.InputIdentifier import Input
from src.backend.PluginManager.ActionInputSupport import ActionInputSupport

from .link import DeckClient
from .actions.deck_action import DeckAction

KEY_ONLY = {
    Input.Key: ActionInputSupport.SUPPORTED,
    Input.Dial: ActionInputSupport.UNSUPPORTED,
    Input.Touchscreen: ActionInputSupport.UNSUPPORTED,
}
KEY_AND_DIAL = {
    Input.Key: ActionInputSupport.SUPPORTED,
    Input.Dial: ActionInputSupport.SUPPORTED,
    Input.Touchscreen: ActionInputSupport.UNSUPPORTED,
}


class NereusSDRPlugin(PluginBase):
    def __init__(self):
        self.PLUGIN_NAME = "NereusSDR"
        self.GITHUB_REPO = "https://github.com/boydsoftprez/NereusSDR"
        super().__init__()

        self.lit = set()
        self.deck_actions = weakref.WeakSet()
        self.link = DeckClient("streamcontroller", "1.0.0",
                               on_state=self._state_changed, on_disconnected=self._link_gone)

        with open(os.path.join(self.PATH, "actions", "actions.json"), encoding="utf-8") as handle:
            actions = json.load(handle)
        for action in actions:
            self.add_action_holder(ActionHolder(
                plugin_base=self,
                # One class per action, so each knows which it is.
                action_base=type("DeckAction_" + action["id"].replace(".", "_"), (DeckAction,),
                                 {"ACTION_ID": action["id"]}),
                action_id_suffix=action["id"].replace(".", "_"),
                action_name=action["name"],
                action_support=KEY_AND_DIAL if action["gesture"] == "turn" else KEY_ONLY,
            ))

        self.register(
            plugin_name="NereusSDR",
            github_repo="https://github.com/boydsoftprez/NereusSDR",
            plugin_version="1.0.0",
            app_version="1.0.0-alpha",
        )
        self.link.start()

    def _state_changed(self, action_id, on):
        if on:
            self.lit.add(action_id)
        else:
            self.lit.discard(action_id)
        for action in list(self.deck_actions):
            if action.ACTION_ID == action_id:
                action.show_light()

    def _link_gone(self):
        self.lit.clear()
        for action in list(self.deck_actions):
            action.show_light()

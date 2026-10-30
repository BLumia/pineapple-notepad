Use the following command to extract strings from kxmlgui rc file:

> [!NOTE]
> Needs to be run at project root, not in `po` directory.

```shell
$ python po/extract_kxmlgui_rc_i18n.py
```

It will extract strings from `po/pineapple-notepadui.rc` and save them to `po/pineapple-notepad.pot`, and also update existing `po` files as well.

For main app translation we currently still use Qt Linguist.

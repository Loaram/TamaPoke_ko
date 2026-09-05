"""SpriteCollab form paths used when a species' base sprite is absent.

Dex numbers remain the game's canonical species IDs.  These paths only choose
which official/recognisable visual form is packed into pNNN.bin; they do not
create extra species, change save data, or change battle data.

The table was audited against SpriteCollab's tracker.json.  Only alternatives
with a real AnimData.xml and an Idle animation are accepted, because Idle is
the minimum required by pack_pmd.py and make_thumbs.py.
"""

# dex: (normal source path, shiny source path, human-readable source form)
PMD_FORM_OVERRIDES = {
    668: ('0000/0000/0002', '0000/0001/0002', 'Female'),
    741: ('0001', '0001/0001', 'Pom_Pom'),
    870: ('0003', '0003/0001', 'Trooper'),
    999: ('0001', '0001/0001', 'Roaming'),
    1008: ('0001', '0001/0001', 'Low_Power'),
}


def sprite_subpath(dexnum, shiny=False):
    """Return the path below sprite/NNNN containing AnimData.xml."""
    override = PMD_FORM_OVERRIDES.get(dexnum)
    if override:
        return override[1 if shiny else 0]
    return '0000/0001' if shiny else ''


def source_form_name(dexnum):
    override = PMD_FORM_OVERRIDES.get(dexnum)
    return override[2] if override else 'Base'

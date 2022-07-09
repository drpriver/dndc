# This is an example of how to externally script a document.
# This script adds the name of the room to the rooms.
import pydndc
import os
# Make sure we're in the right directory
os.chdir(os.path.dirname(__file__))
stuff =[
    ( '1.', "Bedroom"),
    ( '2.', "Butler's Bedroom"),
    ( '3.', "Butler's Pantry"),
    ( '4.', "Plate Safe"),
    ( '5.', "Yard and Garden"),
    ( '6.', "Housekeeper's Room"),
    ( '7.', "Lounge"),
    ( '8.', "Stairs up and Cupboard (under the stairs)"),
    ( '9.', "Arms"),
    ('10.', "Wine Cellar"),
    ('11.', "Water Closet"),
    ('12.', "Larder"),
    ('13.', "Beer"),
    ('14.', "Dairy"),
    ('15.', "Entry Hall"),
    ('16.', "Yard"),
    ('17.', "Scullery"),
    ('18.', "Great Hall"),
    ('19.', "Vestibule"),
    ('20.', "Coat Closet"),
    ('21.', "Stairs Up"),
    ('22.', "Servant's Hall"),
    ('23.', "Kitchen"),
    ('24.', "Stove"),
    ('25.', "Library"),
    ('26.', "Stairs Up"),
    ('27.', "Water Closet"),
    ('28.', "Bed Room"),
    ('29.', "Morning Room"),
    ('30.', "Dungeon"),
    ('31.', "Billiard Room"),
    ('32.', "Lobby"),
    ('33.', "Exterior Stairs up"),
    ('34.', "Unused"),
    ('35.', "The Quadrangle"),
    ('36.', "West Bridge"),
    ('37.', "South Bridge"),
    ('38.', "North Bridge"),
    ('39.', "Bedroom"),
    ('40.', "Dressing Room"),
    ('41.', "Bedroom"),
    ('42.', "Bedroom"),
    ('43.', "Water Closet"),
    ('44.', "Bedroom"),
    ('45.', "Wardrobe"),
    ('46.', "Bedroom"),
    ('47.', "Bedroom"),
    ('48.', "Bedroom"),
    ('49.', "Lounge"),
    ('50.', "Stairs down"),
    ('51.', "Bedroom"),
    ('52.', "Stairs down"),
    ('53.', "Hallway"),
    ('54.', "Closet"),
    ('55.', "Bedroom"),
    ('56.', "Cleric's Closet"),
    ('57.', "Priest's Room"),
    ('58.', "Sunroom"),
    ('59.', "Bird Gallery"),
    ('60.', "Day Nursery"),
    ('61.', "Chapel"),
    ('62.', "Withdrawing Room"),
    ('63.', "Relaxing Room"),
    ('64.', "Down to Dungeon"),
    ('65.', "Dressing Room"),
    ('66.', "Bedroom"),
    ('67.', "Water Closet"),
    ('68.', "Bedroom"),
    ('69.', "Night Nursery"),
]

ctx = pydndc.Context(filename='hobswell-manor-before.dnd')
ctx.logger = pydndc.stderr_logger
ctx.base_dir = '.'
ctx.root.parse_file('hobswell-manor-before.dnd')
for a, b in stuff:
    ctx.node_by_id(a).header = f'{a} {b}'
with open('hobswell-manor.dnd', 'w', newline='') as fp:
    print(ctx.format_tree(), file=fp)

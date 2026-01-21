#!/usr/bin/env python3
"""
Generate RGB565 emoji bitmaps for MeshBerry using Twemoji
Downloads Twemoji PNG files and converts to 12x12 RGB565 bitmaps

Usage: python3 generate_emoji.py > ../src/ui/EmojiData.h

Requires: pip install Pillow requests
"""

from PIL import Image
import sys
import os
import io
import urllib.request
import urllib.error
import ssl
import time

# Twemoji base URL (72x72 PNG files)
TWEMOJI_BASE = "https://raw.githubusercontent.com/twitter/twemoji/master/assets/72x72"

# Cache directory for downloaded files
CACHE_DIR = os.path.join(os.path.dirname(__file__), ".emoji_cache")

# Full emoji set organized by category - comprehensive iPhone/Android compatible set
EMOJI_DATA = {
    "FACES": [
        # Smileys
        (0x1F600, "grin"),           # 😀
        (0x1F603, "smiley"),         # 😃
        (0x1F604, "smile"),          # 😄
        (0x1F601, "grin_sweat"),     # 😁
        (0x1F606, "laugh"),          # 😆
        (0x1F605, "sweat_smile"),    # 😅
        (0x1F923, "rofl"),           # 🤣
        (0x1F602, "joy"),            # 😂
        (0x1F642, "slight_smile"),   # 🙂
        (0x1F643, "upside_down"),    # 🙃
        (0x1F609, "wink"),           # 😉
        (0x1F60A, "blush"),          # 😊
        (0x1F607, "innocent"),       # 😇
        (0x1F970, "smiling_hearts"), # 🥰
        (0x1F60D, "heart_eyes"),     # 😍
        (0x1F929, "star_struck"),    # 🤩
        (0x1F618, "kiss"),           # 😘
        (0x1F617, "kissing"),        # 😗
        (0x263A, "relaxed"),         # ☺️
        (0x1F61A, "kiss_closed"),    # 😚
        (0x1F619, "kiss_smiling"),   # 😙
        (0x1F972, "smiling_tear"),   # 🥲
        (0x1F60B, "yum"),            # 😋
        (0x1F61B, "tongue_out"),     # 😛
        (0x1F61C, "wink_tongue"),    # 😜
        (0x1F92A, "zany"),           # 🤪
        (0x1F61D, "squint_tongue"),  # 😝
        (0x1F911, "money_mouth"),    # 🤑
        (0x1F917, "hugs"),           # 🤗
        (0x1F92D, "hand_mouth"),     # 🤭
        (0x1F92B, "shushing"),       # 🤫
        (0x1F914, "thinking"),       # 🤔
        (0x1F910, "zipper"),         # 🤐
        (0x1F928, "raised_brow"),    # 🤨
        (0x1F610, "neutral"),        # 😐
        (0x1F611, "expressionless"), # 😑
        (0x1F636, "no_mouth"),       # 😶
        (0x1F60F, "smirk"),          # 😏
        (0x1F612, "unamused"),       # 😒
        (0x1F644, "eye_roll"),       # 🙄
        (0x1F62C, "grimace"),        # 😬
        (0x1F925, "lying"),          # 🤥
        (0x1F60C, "relieved"),       # 😌
        (0x1F614, "pensive"),        # 😔
        (0x1F62A, "sleepy"),         # 😪
        (0x1F924, "drooling"),       # 🤤
        (0x1F634, "sleeping"),       # 😴
        (0x1F637, "mask"),           # 😷
        (0x1F912, "thermometer"),    # 🤒
        (0x1F915, "bandage"),        # 🤕
        (0x1F922, "nauseated"),      # 🤢
        (0x1F92E, "vomiting"),       # 🤮
        (0x1F927, "sneezing"),       # 🤧
        (0x1F975, "hot"),            # 🥵
        (0x1F976, "cold"),           # 🥶
        (0x1F974, "woozy"),          # 🥴
        (0x1F635, "dizzy_face"),     # 😵
        (0x1F92F, "exploding"),      # 🤯
        (0x1F920, "cowboy"),         # 🤠
        (0x1F973, "party"),          # 🥳
        (0x1F978, "disguised"),      # 🥸
        (0x1F60E, "sunglasses"),     # 😎
        (0x1F913, "nerd"),           # 🤓
        (0x1F9D0, "monocle"),        # 🧐
        (0x1F615, "confused"),       # 😕
        (0x1F61F, "worried"),        # 😟
        (0x1F641, "frown"),          # 🙁
        (0x2639, "sad"),             # ☹️
        (0x1F62E, "open_mouth"),     # 😮
        (0x1F62F, "hushed"),         # 😯
        (0x1F632, "astonished"),     # 😲
        (0x1F633, "flushed"),        # 😳
        (0x1F97A, "pleading"),       # 🥺
        (0x1F626, "frowning"),       # 😦
        (0x1F627, "anguished"),      # 😧
        (0x1F628, "fearful"),        # 😨
        (0x1F630, "anxious"),        # 😰
        (0x1F625, "disappointed"),   # 😥
        (0x1F622, "cry"),            # 😢
        (0x1F62D, "sob"),            # 😭
        (0x1F631, "scream"),         # 😱
        (0x1F616, "confounded"),     # 😖
        (0x1F623, "persevere"),      # 😣
        (0x1F61E, "disappointed2"),  # 😞
        (0x1F613, "sweat"),          # 😓
        (0x1F629, "weary"),          # 😩
        (0x1F62B, "tired"),          # 😫
        (0x1F971, "yawning"),        # 🥱
        (0x1F624, "triumph"),        # 😤
        (0x1F621, "rage"),           # 😡
        (0x1F620, "angry"),          # 😠
        (0x1F92C, "cursing"),        # 🤬
        (0x1F608, "smiling_imp"),    # 😈
        (0x1F47F, "imp"),            # 👿
        (0x1F480, "skull"),          # 💀
        (0x2620, "skull_bones"),     # ☠️
        (0x1F4A9, "poop"),           # 💩
        (0x1F921, "clown"),          # 🤡
        (0x1F479, "ogre"),           # 👹
        (0x1F47A, "goblin"),         # 👺
        (0x1F47B, "ghost"),          # 👻
        (0x1F47D, "alien"),          # 👽
        (0x1F47E, "space_invader"),  # 👾
        (0x1F916, "robot"),          # 🤖
        (0x1F63A, "smiley_cat"),     # 😺
        (0x1F638, "smile_cat"),      # 😸
        (0x1F639, "joy_cat"),        # 😹
        (0x1F63B, "heart_eyes_cat"), # 😻
        (0x1F63C, "smirk_cat"),      # 😼
        (0x1F63D, "kiss_cat"),       # 😽
        (0x1F640, "scream_cat"),     # 🙀
        (0x1F63F, "cry_cat"),        # 😿
        (0x1F63E, "angry_cat"),      # 😾
    ],
    "GESTURES": [
        (0x1F44B, "wave"),           # 👋
        (0x1F91A, "raised_back"),    # 🤚
        (0x1F590, "raised_hand"),    # 🖐️
        (0x270B, "hand"),            # ✋
        (0x1F596, "vulcan"),         # 🖖
        (0x1F44C, "ok_hand"),        # 👌
        (0x1F90C, "pinched"),        # 🤌
        (0x1F90F, "pinching"),       # 🤏
        (0x270C, "v"),               # ✌️
        (0x1F91E, "crossed_fingers"),  # 🤞
        (0x1F91F, "love_you"),       # 🤟
        (0x1F918, "horns"),          # 🤘
        (0x1F919, "call_me"),        # 🤙
        (0x1F448, "point_left"),     # 👈
        (0x1F449, "point_right"),    # 👉
        (0x1F446, "point_up2"),      # 👆
        (0x1F595, "middle_finger"),  # 🖕
        (0x1F447, "point_down"),     # 👇
        (0x261D, "point_up"),        # ☝️
        (0x1F44D, "thumbsup"),       # 👍
        (0x1F44E, "thumbsdown"),     # 👎
        (0x270A, "fist"),            # ✊
        (0x1F44A, "punch"),          # 👊
        (0x1F91B, "left_fist"),      # 🤛
        (0x1F91C, "right_fist"),     # 🤜
        (0x1F44F, "clap"),           # 👏
        (0x1F64C, "raised_hands"),   # 🙌
        (0x1F450, "open_hands"),     # 👐
        (0x1F932, "palms_up"),       # 🤲
        (0x1F91D, "handshake"),      # 🤝
        (0x1F64F, "pray"),           # 🙏
        (0x270D, "writing"),         # ✍️
        (0x1F485, "nail_polish"),    # 💅
        (0x1F933, "selfie"),         # 🤳
        (0x1F4AA, "muscle"),         # 💪
        (0x1F9BE, "mech_arm"),       # 🦾
        (0x1F9BF, "mech_leg"),       # 🦿
        (0x1F9B5, "leg"),            # 🦵
        (0x1F9B6, "foot"),           # 🦶
        (0x1F442, "ear"),            # 👂
        (0x1F9BB, "ear_aid"),        # 🦻
        (0x1F443, "nose"),           # 👃
        (0x1F9E0, "brain"),          # 🧠
        (0x1F9B7, "tooth"),          # 🦷
        (0x1F9B4, "bone"),           # 🦴
        (0x1F440, "eyes"),           # 👀
        (0x1F441, "eye"),            # 👁️
        (0x1F445, "tongue"),         # 👅
        (0x1F444, "lips"),           # 👄
    ],
    "PEOPLE": [
        (0x1F476, "baby"),           # 👶
        (0x1F9D2, "child"),          # 🧒
        (0x1F466, "boy"),            # 👦
        (0x1F467, "girl"),           # 👧
        (0x1F9D1, "person"),         # 🧑
        (0x1F471, "blond"),          # 👱
        (0x1F468, "man"),            # 👨
        (0x1F9D4, "beard"),          # 🧔
        (0x1F469, "woman"),          # 👩
        (0x1F9D3, "older_person"),   # 🧓
        (0x1F474, "old_man"),        # 👴
        (0x1F475, "old_woman"),      # 👵
        (0x1F64D, "person_frown"),   # 🙍
        (0x1F64E, "person_pout"),    # 🙎
        (0x1F645, "no_good"),        # 🙅
        (0x1F646, "ok_person"),      # 🙆
        (0x1F481, "tipping_hand"),   # 💁
        (0x1F64B, "raising_hand"),   # 🙋
        (0x1F9CF, "deaf_person"),    # 🧏
        (0x1F647, "person_bow"),     # 🙇
        (0x1F926, "facepalm"),       # 🤦
        (0x1F937, "shrug"),          # 🤷
        (0x1F46E, "cop"),            # 👮
        (0x1F575, "detective"),      # 🕵️
        (0x1F482, "guard"),          # 💂
        (0x1F977, "ninja"),          # 🥷
        (0x1F477, "construction_worker"),  # 👷
        (0x1F934, "prince"),         # 🤴
        (0x1F478, "princess"),       # 👸
        (0x1F473, "turban"),         # 👳
        (0x1F472, "man_cap"),        # 👲
        (0x1F9D5, "headscarf"),      # 🧕
        (0x1F935, "tuxedo"),         # 🤵
        (0x1F470, "bride"),          # 👰
        (0x1F930, "pregnant"),       # 🤰
        (0x1F931, "breastfeeding"),  # 🤱
        (0x1F47C, "angel"),          # 👼
        (0x1F385, "santa"),          # 🎅
        (0x1F936, "mrs_claus"),      # 🤶
        (0x1F9B8, "superhero"),      # 🦸
        (0x1F9B9, "supervillain"),   # 🦹
        (0x1F9D9, "mage"),           # 🧙
        (0x1F9DA, "fairy"),          # 🧚
        (0x1F9DB, "vampire"),        # 🧛
        (0x1F9DC, "merperson"),      # 🧜
        (0x1F9DD, "elf"),            # 🧝
        (0x1F9DE, "genie"),          # 🧞
        (0x1F9DF, "zombie"),         # 🧟
    ],
    "HEARTS": [
        (0x2764, "heart"),           # ❤️
        (0x1F9E1, "orange_heart"),   # 🧡
        (0x1F49B, "yellow_heart"),   # 💛
        (0x1F49A, "green_heart"),    # 💚
        (0x1F499, "blue_heart"),     # 💙
        (0x1F49C, "purple_heart"),   # 💜
        (0x1F90E, "brown_heart"),    # 🤎
        (0x1F5A4, "black_heart"),    # 🖤
        (0x1F90D, "white_heart"),    # 🤍
        (0x1F494, "broken_heart"),   # 💔
        (0x2763, "heart_excl"),      # ❣️
        (0x1F495, "two_hearts"),     # 💕
        (0x1F49E, "revolving"),      # 💞
        (0x1F493, "heartbeat"),      # 💓
        (0x1F497, "heartpulse"),     # 💗
        (0x1F496, "sparkling"),      # 💖
        (0x1F498, "cupid"),          # 💘
        (0x1F49D, "gift_heart"),     # 💝
        (0x1F49F, "heart_decor"),    # 💟
        (0x2665, "hearts_suit"),     # ♥️
        (0x1F48B, "kiss_mark"),      # 💋
        (0x1F48C, "love_letter"),    # 💌
        (0x1F48D, "ring"),           # 💍
        (0x1F48E, "gem"),            # 💎
        (0x1F490, "bouquet"),        # 💐
        (0x1F339, "rose"),           # 🌹
        (0x1F940, "wilted"),         # 🥀
        (0x1F33A, "hibiscus"),       # 🌺
        (0x1F337, "tulip"),          # 🌷
        (0x1F338, "cherry_blossom"), # 🌸
    ],
    "ANIMALS": [
        (0x1F436, "dog"),            # 🐶
        (0x1F431, "cat"),            # 🐱
        (0x1F42D, "mouse"),          # 🐭
        (0x1F439, "hamster"),        # 🐹
        (0x1F430, "rabbit"),         # 🐰
        (0x1F98A, "fox"),            # 🦊
        (0x1F43B, "bear"),           # 🐻
        (0x1F43C, "panda"),          # 🐼
        (0x1F428, "koala"),          # 🐨
        (0x1F42F, "tiger"),          # 🐯
        (0x1F981, "lion"),           # 🦁
        (0x1F42E, "cow"),            # 🐮
        (0x1F437, "pig"),            # 🐷
        (0x1F438, "frog"),           # 🐸
        (0x1F435, "monkey"),         # 🐵
        (0x1F648, "see_no_evil"),    # 🙈
        (0x1F649, "hear_no_evil"),   # 🙉
        (0x1F64A, "speak_no_evil"),  # 🙊
        (0x1F412, "monkey2"),        # 🐒
        (0x1F414, "chicken"),        # 🐔
        (0x1F427, "penguin"),        # 🐧
        (0x1F426, "bird"),           # 🐦
        (0x1F424, "chick"),          # 🐤
        (0x1F986, "duck"),           # 🦆
        (0x1F985, "eagle"),          # 🦅
        (0x1F989, "owl"),            # 🦉
        (0x1F987, "bat"),            # 🦇
        (0x1F43A, "wolf"),           # 🐺
        (0x1F417, "boar"),           # 🐗
        (0x1F434, "horse"),          # 🐴
        (0x1F984, "unicorn"),        # 🦄
        (0x1F41D, "bee"),            # 🐝
        (0x1F41B, "bug"),            # 🐛
        (0x1F98B, "butterfly"),      # 🦋
        (0x1F40C, "snail"),          # 🐌
        (0x1F41E, "ladybug"),        # 🐞
        (0x1F41C, "ant"),            # 🐜
        (0x1F422, "turtle"),         # 🐢
        (0x1F40D, "snake"),          # 🐍
        (0x1F409, "dragon"),         # 🐉
        (0x1F432, "dragon_face"),    # 🐲
        (0x1F995, "sauropod"),       # 🦕
        (0x1F996, "t_rex"),          # 🦖
        (0x1F433, "whale"),          # 🐳
        (0x1F42C, "dolphin"),        # 🐬
        (0x1F41F, "fish"),           # 🐟
        (0x1F420, "trop_fish"),      # 🐠
        (0x1F421, "blowfish"),       # 🐡
        (0x1F988, "shark"),          # 🦈
        (0x1F419, "octopus"),        # 🐙
        (0x1F41A, "shell"),          # 🐚
        (0x1F40B, "whale2"),         # 🐋
        (0x1F40A, "crocodile"),      # 🐊
        (0x1F406, "leopard"),        # 🐆
        (0x1F405, "tiger2"),         # 🐅
        (0x1F403, "water_buffalo"),  # 🐃
        (0x1F402, "ox"),             # 🐂
        (0x1F404, "cow2"),           # 🐄
        (0x1F98C, "deer"),           # 🦌
        (0x1F42A, "camel"),          # 🐪
        (0x1F42B, "camel2"),         # 🐫
        (0x1F999, "llama"),          # 🦙
        (0x1F992, "giraffe"),        # 🦒
        (0x1F418, "elephant"),       # 🐘
        (0x1F98F, "rhino"),          # 🦏
        (0x1F99B, "hippo"),          # 🦛
        (0x1F401, "mouse2"),         # 🐁
        (0x1F400, "rat"),            # 🐀
        (0x1F407, "rabbit2"),        # 🐇
        (0x1F43F, "chipmunk"),       # 🐿️
        (0x1F994, "hedgehog"),       # 🦔
        (0x1F9A1, "badger"),         # 🦡
        (0x1F43E, "paw_prints"),     # 🐾
    ],
    "FOOD": [
        (0x1F34E, "apple"),          # 🍎
        (0x1F34F, "green_apple"),    # 🍏
        (0x1F350, "pear"),           # 🍐
        (0x1F34A, "orange"),         # 🍊
        (0x1F34B, "lemon"),          # 🍋
        (0x1F34C, "banana"),         # 🍌
        (0x1F349, "watermelon"),     # 🍉
        (0x1F347, "grapes"),         # 🍇
        (0x1F353, "strawberry"),     # 🍓
        (0x1FAD0, "blueberries"),    # 🫐
        (0x1F352, "cherries"),       # 🍒
        (0x1F351, "peach"),          # 🍑
        (0x1F96D, "mango"),          # 🥭
        (0x1F34D, "pineapple"),      # 🍍
        (0x1F965, "coconut"),        # 🥥
        (0x1F95D, "kiwi"),           # 🥝
        (0x1F345, "tomato"),         # 🍅
        (0x1F346, "eggplant"),       # 🍆
        (0x1F951, "avocado"),        # 🥑
        (0x1F966, "broccoli"),       # 🥦
        (0x1F96C, "leafy_green"),    # 🥬
        (0x1F952, "cucumber"),       # 🥒
        (0x1F336, "hot_pepper"),     # 🌶️
        (0x1F33D, "corn"),           # 🌽
        (0x1F955, "carrot"),         # 🥕
        (0x1F9C4, "garlic"),         # 🧄
        (0x1F9C5, "onion"),          # 🧅
        (0x1F954, "potato"),         # 🥔
        (0x1F360, "potato2"),        # 🍠
        (0x1F950, "croissant"),      # 🥐
        (0x1F35E, "bread"),          # 🍞
        (0x1F956, "baguette"),       # 🥖
        (0x1FAD3, "flatbread"),      # 🫓
        (0x1F968, "pretzel"),        # 🥨
        (0x1F96F, "bagel"),          # 🥯
        (0x1F95E, "pancakes"),       # 🥞
        (0x1F9C7, "waffle"),         # 🧇
        (0x1F9C0, "cheese"),         # 🧀
        (0x1F356, "meat"),           # 🍖
        (0x1F357, "poultry"),        # 🍗
        (0x1F969, "steak"),          # 🥩
        (0x1F953, "bacon"),          # 🥓
        (0x1F354, "hamburger"),      # 🍔
        (0x1F35F, "fries"),          # 🍟
        (0x1F355, "pizza"),          # 🍕
        (0x1F32D, "hotdog"),         # 🌭
        (0x1F96A, "sandwich"),       # 🥪
        (0x1F32E, "taco"),           # 🌮
        (0x1F32F, "burrito"),        # 🌯
        (0x1FAD4, "tamale"),         # 🫔
        (0x1F959, "falafel"),        # 🥙
        (0x1F95A, "egg"),            # 🥚
        (0x1F373, "cooking"),        # 🍳
        (0x1F958, "paella"),         # 🥘
        (0x1F372, "stew"),           # 🍲
        (0x1FAD5, "fondue"),         # 🫕
        (0x1F963, "bowl"),           # 🥣
        (0x1F957, "salad"),          # 🥗
        (0x1F37F, "popcorn"),        # 🍿
        (0x1F9C8, "butter"),         # 🧈
        (0x1F9C2, "salt"),           # 🧂
        (0x1F35C, "ramen"),          # 🍜
        (0x1F35D, "spaghetti"),      # 🍝
        (0x1F35B, "curry"),          # 🍛
        (0x1F35A, "rice"),           # 🍚
        (0x1F359, "rice_ball"),      # 🍙
        (0x1F358, "rice_cracker"),   # 🍘
        (0x1F365, "fish_cake"),      # 🍥
        (0x1F960, "fortune"),        # 🥠
        (0x1F961, "takeout"),        # 🥡
        (0x1F366, "icecream"),       # 🍦
        (0x1F367, "shaved_ice"),     # 🍧
        (0x1F368, "ice_cream"),      # 🍨
        (0x1F369, "doughnut"),       # 🍩
        (0x1F36A, "cookie"),         # 🍪
        (0x1F382, "birthday"),       # 🎂
        (0x1F370, "cake"),           # 🍰
        (0x1F9C1, "cupcake"),        # 🧁
        (0x1F967, "pie"),            # 🥧
        (0x1F36B, "chocolate"),      # 🍫
        (0x1F36C, "candy"),          # 🍬
        (0x1F36D, "lollipop"),       # 🍭
        (0x1F36E, "custard"),        # 🍮
        (0x1F36F, "honey"),          # 🍯
        (0x1F37C, "bottle"),         # 🍼
        (0x1F95B, "milk"),           # 🥛
        (0x2615, "coffee"),          # ☕
        (0x1FAD6, "teapot"),         # 🫖
        (0x1F375, "tea"),            # 🍵
        (0x1F376, "sake"),           # 🍶
        (0x1F37E, "champagne"),      # 🍾
        (0x1F377, "wine"),           # 🍷
        (0x1F378, "cocktail"),       # 🍸
        (0x1F379, "tropical"),       # 🍹
        (0x1F37A, "beer"),           # 🍺
        (0x1F37B, "beers"),          # 🍻
        (0x1F942, "clinking"),       # 🥂
        (0x1F943, "tumbler"),        # 🥃
        (0x1F964, "cup_straw"),      # 🥤
        (0x1F9CB, "bubble_tea"),     # 🧋
        (0x1F9C3, "juice"),          # 🧃
        (0x1F9C9, "mate"),           # 🧉
        (0x1F9CA, "ice"),            # 🧊
    ],
    "ACTIVITIES": [
        (0x26BD, "soccer"),          # ⚽
        (0x1F3C0, "basketball"),     # 🏀
        (0x1F3C8, "football"),       # 🏈
        (0x26BE, "baseball"),        # ⚾
        (0x1F94E, "softball"),       # 🥎
        (0x1F3BE, "tennis"),         # 🎾
        (0x1F3D0, "volleyball"),     # 🏐
        (0x1F3C9, "rugby"),          # 🏉
        (0x1F94F, "flying_disc"),    # 🥏
        (0x1F3B1, "billiards"),      # 🎱
        (0x1F3D3, "ping_pong"),      # 🏓
        (0x1F3F8, "badminton"),      # 🏸
        (0x1F3D2, "hockey"),         # 🏒
        (0x1F3D1, "field_hockey"),   # 🏑
        (0x1F94D, "lacrosse"),       # 🥍
        (0x1F3CF, "cricket"),        # 🏏
        (0x1F945, "goal"),           # 🥅
        (0x26F3, "golf"),            # ⛳
        (0x1F3F9, "bow_arrow"),      # 🏹
        (0x1F3A3, "fishing"),        # 🎣
        (0x1F93F, "diving"),         # 🤿
        (0x1F3BD, "running_shirt"),  # 🎽
        (0x1F6F9, "skateboard"),     # 🛹
        (0x1F6FC, "roller_skate"),   # 🛼
        (0x1F94C, "curling"),        # 🥌
        (0x26F7, "ski"),             # ⛷️
        (0x1F3BF, "skis"),           # 🎿
        (0x1F3C2, "snowboard"),      # 🏂
        (0x1F3CB, "weight_lift"),    # 🏋️
        (0x1F93C, "wrestling"),      # 🤼
        (0x1F938, "cartwheeling"),   # 🤸
        (0x1F93A, "fencing"),        # 🤺
        (0x1F93E, "handball"),       # 🤾
        (0x1F3CC, "golfing"),        # 🏌️
        (0x1F3C7, "horse_racing"),   # 🏇
        (0x1F9D8, "yoga"),           # 🧘
        (0x1F3AF, "dart"),           # 🎯
        (0x1FA80, "yoyo"),           # 🪀
        (0x1FA81, "kite"),           # 🪁
        (0x1F3B0, "slot"),           # 🎰
        (0x1F3B2, "dice"),           # 🎲
        (0x1F9E9, "puzzle"),         # 🧩
        (0x1F9F8, "teddy"),          # 🧸
        (0x1FA86, "nesting"),        # 🪆
        (0x2660, "spades"),          # ♠️
        (0x2665, "hearts"),          # ♥️
        (0x2666, "diamonds"),        # ♦️
        (0x2663, "clubs"),           # ♣️
        (0x265F, "chess"),           # ♟️
        (0x1F0CF, "joker"),          # 🃏
        (0x1F004, "mahjong"),        # 🀄
        (0x1F3AD, "masks"),          # 🎭
        (0x1F3A8, "art"),            # 🎨
        (0x1F3C6, "trophy"),         # 🏆
        (0x1F3C5, "medal"),          # 🏅
        (0x1F947, "first_place"),    # 🥇
        (0x1F948, "second_place"),   # 🥈
        (0x1F949, "third_place"),    # 🥉
        (0x1F94A, "boxing"),         # 🥊
        (0x1F94B, "martial_arts"),   # 🥋
        (0x1F3AE, "video_game"),     # 🎮
        (0x1F579, "joystick"),       # 🕹️
        (0x1F3B9, "piano"),          # 🎹
        (0x1F3B7, "saxophone"),      # 🎷
        (0x1F3BA, "trumpet"),        # 🎺
        (0x1F3B8, "guitar"),         # 🎸
        (0x1FA95, "banjo"),          # 🪕
        (0x1F3BB, "violin"),         # 🎻
        (0x1FA98, "accordion"),      # 🪘
        (0x1F941, "drum"),           # 🥁
        (0x1FA97, "maracas"),        # 🪇
        (0x1F3BC, "music_score"),    # 🎼
        (0x1F3A4, "microphone"),     # 🎤
        (0x1F3A7, "headphones"),     # 🎧
        (0x1F4FB, "radio"),          # 📻
    ],
    "TRAVEL": [
        (0x1F697, "car"),            # 🚗
        (0x1F695, "taxi"),           # 🚕
        (0x1F699, "suv"),            # 🚙
        (0x1F68C, "bus"),            # 🚌
        (0x1F68E, "trolley"),        # 🚎
        (0x1F3CE, "race_car"),       # 🏎️
        (0x1F693, "police_car"),     # 🚓
        (0x1F691, "ambulance"),      # 🚑
        (0x1F692, "fire_engine"),    # 🚒
        (0x1F690, "minibus"),        # 🚐
        (0x1F6FB, "pickup"),         # 🛻
        (0x1F69A, "truck"),          # 🚚
        (0x1F69B, "semi"),           # 🚛
        (0x1F69C, "tractor"),        # 🚜
        (0x1F3CD, "motorcycle"),     # 🏍️
        (0x1F6F5, "scooter"),        # 🛵
        (0x1F6B2, "bicycle"),        # 🚲
        (0x1F6F4, "kick_scooter"),   # 🛴
        (0x1F6FA, "auto_rick"),      # 🛺
        (0x1F6A8, "police_light"),   # 🚨
        (0x1F694, "police_car2"),    # 🚔
        (0x1F68D, "bus2"),           # 🚍
        (0x1F698, "car2"),           # 🚘
        (0x1F696, "taxi2"),          # 🚖
        (0x1F682, "train"),          # 🚂
        (0x1F683, "railway"),        # 🚃
        (0x1F684, "bullet"),         # 🚄
        (0x1F685, "bullet2"),        # 🚅
        (0x1F686, "train2"),         # 🚆
        (0x1F687, "metro"),          # 🚇
        (0x1F688, "light_rail"),     # 🚈
        (0x1F689, "station"),        # 🚉
        (0x1F68A, "tram"),           # 🚊
        (0x1F69D, "monorail"),       # 🚝
        (0x1F69E, "mountain_rail"),  # 🚞
        (0x1F69F, "suspension"),     # 🚟
        (0x1F6A0, "aerial"),         # 🚠
        (0x1F6A1, "gondola"),        # 🚡
        (0x1F681, "helicopter"),     # 🚁
        (0x2708, "airplane"),        # ✈️
        (0x1F6E9, "small_plane"),    # 🛩️
        (0x1F6EB, "departure"),      # 🛫
        (0x1F6EC, "arrival"),        # 🛬
        (0x1FA82, "parachute"),      # 🪂
        (0x1F4BA, "seat"),           # 💺
        (0x1F680, "rocket"),         # 🚀
        (0x1F6F8, "ufo"),            # 🛸
        (0x1F6F0, "satellite"),      # 🛰️
        (0x1F6A2, "ship"),           # 🚢
        (0x26F5, "sailboat"),        # ⛵
        (0x1F6F6, "canoe"),          # 🛶
        (0x1F6A4, "speedboat"),      # 🚤
        (0x1F6F3, "ferry"),          # 🛳️
        (0x26F4, "ferry2"),          # ⛴️
        (0x1F6A3, "rowing"),         # 🚣
        (0x2693, "anchor"),          # ⚓
        (0x26FD, "fuel"),            # ⛽
        (0x1F6A7, "construction"),   # 🚧
        (0x1F6A6, "traffic"),        # 🚦
        (0x1F6A5, "traffic2"),       # 🚥
        (0x1F68F, "bus_stop"),       # 🚏
        (0x1F5FA, "world_map"),      # 🗺️
        (0x1F5FF, "moyai"),          # 🗿
        (0x1F5FD, "liberty"),        # 🗽
        (0x1F5FC, "tokyo_tower"),    # 🗼
        (0x1F3F0, "castle"),         # 🏰
        (0x1F3EF, "japanese_castle"),# 🏯
        (0x1F3E0, "house"),          # 🏠
        (0x1F3E1, "house_garden"),   # 🏡
        (0x1F3E2, "office"),         # 🏢
        (0x1F3E3, "post_office"),    # 🏣
        (0x1F3E4, "post_office2"),   # 🏤
        (0x1F3E5, "hospital"),       # 🏥
        (0x1F3E6, "bank"),           # 🏦
        (0x1F3E8, "hotel"),          # 🏨
        (0x1F3E9, "love_hotel"),     # 🏩
        (0x1F3EA, "store"),          # 🏪
        (0x1F3EB, "school"),         # 🏫
        (0x1F3EC, "department"),     # 🏬
        (0x1F3ED, "factory"),        # 🏭
        (0x26EA, "church"),          # ⛪
        (0x1F54C, "mosque"),         # 🕌
        (0x1F54D, "synagogue"),      # 🕍
        (0x26E9, "shinto"),          # ⛩️
        (0x1F54B, "kaaba"),          # 🕋
        (0x26F2, "fountain"),        # ⛲
        (0x26FA, "tent"),            # ⛺
        (0x1F30B, "volcano"),        # 🌋
        (0x1F3D4, "mountain_snow"),  # 🏔️
        (0x1F3D5, "camping"),        # 🏕️
        (0x1F3D6, "beach"),          # 🏖️
        (0x1F3DC, "desert"),         # 🏜️
        (0x1F3DD, "island"),         # 🏝️
        (0x1F3DE, "park"),           # 🏞️
        (0x1F3DF, "stadium"),        # 🏟️
        (0x1F3DB, "classical"),      # 🏛️
        (0x1F3DA, "derelict"),       # 🏚️
        (0x1F3D7, "construction2"),  # 🏗️
        (0x1F3D8, "houses"),         # 🏘️
        (0x1F3D9, "cityscape"),      # 🏙️
    ],
    "OBJECTS": [
        (0x231A, "watch"),           # ⌚
        (0x1F4F1, "phone"),          # 📱
        (0x1F4F2, "calling"),        # 📲
        (0x1F4BB, "laptop"),         # 💻
        (0x2328, "keyboard"),        # ⌨️
        (0x1F5A5, "computer"),       # 🖥️
        (0x1F5A8, "printer"),        # 🖨️
        (0x1F5B1, "computer_mouse"), # 🖱️
        (0x1F5B2, "trackball"),      # 🖲️
        (0x1F4BD, "minidisc"),       # 💽
        (0x1F4BE, "floppy"),         # 💾
        (0x1F4BF, "cd"),             # 💿
        (0x1F4C0, "dvd"),            # 📀
        (0x1F9EE, "abacus"),         # 🧮
        (0x1F3A5, "film"),           # 🎥
        (0x1F39E, "film_frames"),    # 🎞️
        (0x1F4FD, "projector"),      # 📽️
        (0x1F3AC, "clapper"),        # 🎬
        (0x1F4F7, "camera"),         # 📷
        (0x1F4F8, "camera_flash"),   # 📸
        (0x1F4F9, "video_camera"),   # 📹
        (0x1F4FC, "vhs"),            # 📼
        (0x1F50D, "mag"),            # 🔍
        (0x1F50E, "mag_right"),      # 🔎
        (0x1F56F, "candle"),         # 🕯️
        (0x1F4A1, "bulb"),           # 💡
        (0x1F526, "flashlight"),     # 🔦
        (0x1F3EE, "lantern"),        # 🏮
        (0x1FA94, "lamp"),           # 🪔
        (0x1F4D4, "notebook"),       # 📔
        (0x1F4D5, "book_closed"),    # 📕
        (0x1F4D6, "book_open"),      # 📖
        (0x1F4D7, "green_book"),     # 📗
        (0x1F4D8, "blue_book"),      # 📘
        (0x1F4D9, "orange_book"),    # 📙
        (0x1F4DA, "books"),          # 📚
        (0x1F4D3, "notebook2"),      # 📓
        (0x1F4D2, "ledger"),         # 📒
        (0x1F4C3, "page_curl"),      # 📃
        (0x1F4DC, "scroll"),         # 📜
        (0x1F4C4, "page"),           # 📄
        (0x1F4F0, "newspaper"),      # 📰
        (0x1F5DE, "newspaper2"),     # 🗞️
        (0x1F4D1, "bookmark_tabs"),  # 📑
        (0x1F516, "bookmark"),       # 🔖
        (0x1F3F7, "label"),          # 🏷️
        (0x1F4B0, "money_bag"),      # 💰
        (0x1FA99, "coin"),           # 🪙
        (0x1F4B4, "yen"),            # 💴
        (0x1F4B5, "dollar"),         # 💵
        (0x1F4B6, "euro"),           # 💶
        (0x1F4B7, "pound"),          # 💷
        (0x1F4B8, "money_wings"),    # 💸
        (0x1F4B3, "credit_card"),    # 💳
        (0x1F9FE, "receipt"),        # 🧾
        (0x1F4B9, "chart"),          # 💹
        (0x2709, "envelope"),        # ✉️
        (0x1F4E7, "email"),          # 📧
        (0x1F4E8, "incoming"),       # 📨
        (0x1F4E9, "outbox"),         # 📩
        (0x1F4E4, "outbox2"),        # 📤
        (0x1F4E5, "inbox"),          # 📥
        (0x1F4E6, "package"),        # 📦
        (0x1F4EB, "mailbox"),        # 📫
        (0x1F4EA, "mailbox2"),       # 📪
        (0x1F4EC, "mailbox3"),       # 📬
        (0x1F4ED, "mailbox4"),       # 📭
        (0x1F4EE, "postbox"),        # 📮
        (0x1F5F3, "ballot"),         # 🗳️
        (0x270F, "pencil"),          # ✏️
        (0x2712, "nib"),             # ✒️
        (0x1F58B, "pen"),            # 🖋️
        (0x1F58A, "pen2"),           # 🖊️
        (0x1F58C, "brush"),          # 🖌️
        (0x1F58D, "crayon"),         # 🖍️
        (0x1F4DD, "memo"),           # 📝
        (0x1F4BC, "briefcase"),      # 💼
        (0x1F4C1, "folder"),         # 📁
        (0x1F4C2, "folder_open"),    # 📂
        (0x1F5C2, "dividers"),       # 🗂️
        (0x1F4C5, "calendar"),       # 📅
        (0x1F4C6, "calendar2"),      # 📆
        (0x1F5D2, "spiral_note"),    # 🗒️
        (0x1F5D3, "spiral_cal"),     # 🗓️
        (0x1F4C7, "rolodex"),        # 📇
        (0x1F4C8, "chart_up"),       # 📈
        (0x1F4C9, "chart_down"),     # 📉
        (0x1F4CA, "bar_chart"),      # 📊
        (0x1F4CB, "clipboard"),      # 📋
        (0x1F4CC, "pushpin"),        # 📌
        (0x1F4CD, "pin"),            # 📍
        (0x1F4CE, "paperclip"),      # 📎
        (0x1F587, "paperclips"),     # 🖇️
        (0x1F4CF, "ruler"),          # 📏
        (0x1F4D0, "ruler2"),         # 📐
        (0x2702, "scissors"),        # ✂️
        (0x1F5C3, "card_box"),       # 🗃️
        (0x1F5C4, "cabinet"),        # 🗄️
        (0x1F5D1, "wastebasket"),    # 🗑️
        (0x1F512, "lock"),           # 🔒
        (0x1F513, "unlock"),         # 🔓
        (0x1F50F, "lock_pen"),       # 🔏
        (0x1F510, "lock_key"),       # 🔐
        (0x1F511, "key"),            # 🔑
        (0x1F5DD, "old_key"),        # 🗝️
        (0x1F528, "hammer"),         # 🔨
        (0x1FA93, "axe"),            # 🪓
        (0x26CF, "pick"),            # ⛏️
        (0x2692, "hammer_pick"),     # ⚒️
        (0x1F6E0, "tools"),          # 🛠️
        (0x1F5E1, "dagger"),         # 🗡️
        (0x2694, "swords"),          # ⚔️
        (0x1F52B, "gun"),            # 🔫
        (0x1FA83, "boomerang"),      # 🪃
        (0x1F3F9, "bow2"),           # 🏹
        (0x1F6E1, "shield"),         # 🛡️
        (0x1FA9A, "carpentry"),      # 🪚
        (0x1F527, "wrench"),         # 🔧
        (0x1FA9B, "screwdriver"),    # 🪛
        (0x1F529, "nut_bolt"),       # 🔩
        (0x2699, "gear"),            # ⚙️
        (0x1F5DC, "clamp"),          # 🗜️
        (0x2696, "scales"),          # ⚖️
        (0x1F9AF, "cane"),           # 🦯
        (0x1F517, "link"),           # 🔗
        (0x26D3, "chains"),          # ⛓️
        (0x1FA9D, "hook"),           # 🪝
        (0x1F9F0, "toolbox"),        # 🧰
        (0x1F9F2, "magnet"),         # 🧲
        (0x1FA9C, "ladder"),         # 🪜
        (0x2697, "alembic"),         # ⚗️
        (0x1F9EA, "test_tube"),      # 🧪
        (0x1F9EB, "petri"),          # 🧫
        (0x1F9EC, "dna"),            # 🧬
        (0x1F52C, "microscope"),     # 🔬
        (0x1F52D, "telescope"),      # 🔭
        (0x1F4E1, "satellite2"),     # 📡
        (0x1F489, "syringe"),        # 💉
        (0x1FA78, "drop_blood"),     # 🩸
        (0x1F48A, "pill"),           # 💊
        (0x1FA79, "adhesive_bandage"),  # 🩹
        (0x1FA7A, "stethoscope"),    # 🩺
        (0x1F6AA, "door"),           # 🚪
        (0x1F6D7, "elevator"),       # 🛗
        (0x1FA9E, "mirror"),         # 🪞
        (0x1FA9F, "window"),         # 🪟
        (0x1F6CF, "bed"),            # 🛏️
        (0x1F6CB, "couch"),          # 🛋️
        (0x1FA91, "chair"),          # 🪑
        (0x1F6BD, "toilet"),         # 🚽
        (0x1FAA0, "plunger"),        # 🪠
        (0x1F6BF, "shower"),         # 🚿
        (0x1F6C1, "bathtub"),        # 🛁
        (0x1FAA4, "mousetrap"),      # 🪤
        (0x1FA92, "razor"),          # 🪒
        (0x1F9F4, "lotion"),         # 🧴
        (0x1F9F7, "safety_pin"),     # 🧷
        (0x1F9F9, "broom"),          # 🧹
        (0x1F9FA, "basket"),         # 🧺
        (0x1F9FB, "roll"),           # 🧻
        (0x1FAA3, "bucket"),         # 🪣
        (0x1F9FC, "soap"),           # 🧼
        (0x1FAE7, "bubbles"),        # 🫧
        (0x1FAA5, "toothbrush"),     # 🪥
        (0x1F9FD, "sponge"),         # 🧽
        (0x1F9EF, "extinguisher"),   # 🧯
        (0x1F6D2, "cart"),           # 🛒
        (0x1F6AC, "cigarette"),      # 🚬
        (0x26B0, "coffin"),          # ⚰️
        (0x1FAA6, "headstone"),      # 🪦
        (0x26B1, "urn"),             # ⚱️
        (0x1F5FF, "moai"),           # 🗿
        (0x1FAA7, "placard"),        # 🪧
        (0x1FAA8, "rock"),           # 🪨
    ],
    "SYMBOLS": [
        (0x2705, "check"),           # ✅
        (0x274C, "cross"),           # ❌
        (0x274E, "cross_neg"),       # ❎
        (0x2795, "plus"),            # ➕
        (0x2796, "minus"),           # ➖
        (0x2797, "divide"),          # ➗
        (0x27B0, "curly_loop"),      # ➰
        (0x27BF, "double_loop"),     # ➿
        (0x2B50, "star"),            # ⭐
        (0x1F31F, "star2"),          # 🌟
        (0x2728, "sparkles"),        # ✨
        (0x1F4AB, "dizzy"),          # 💫
        (0x1F4A5, "boom"),           # 💥
        (0x1F4A2, "anger"),          # 💢
        (0x1F4A6, "sweat_drops"),    # 💦
        (0x1F4A8, "dash"),           # 💨
        (0x1F573, "hole"),           # 🕳️
        (0x1F4AC, "speech"),         # 💬
        (0x1F5E8, "left_speech"),    # 🗨️
        (0x1F5EF, "speech_right"),   # 🗯️
        (0x1F4AD, "thought"),        # 💭
        (0x1F4A4, "zzz"),            # 💤
        (0x1F525, "fire"),           # 🔥
        (0x1F4AF, "100"),            # 💯
        (0x1F389, "tada"),           # 🎉
        (0x1F38A, "confetti"),       # 🎊
        (0x1F388, "balloon"),        # 🎈
        (0x1F381, "gift"),           # 🎁
        (0x1F380, "ribbon"),         # 🎀
        (0x1F397, "reminder"),       # 🎗️
        (0x1F39F, "tickets"),        # 🎟️
        (0x1F3AB, "ticket"),         # 🎫
        (0x1F396, "military"),       # 🎖️
        (0x26A1, "zap"),             # ⚡
        (0x1F300, "cyclone"),        # 🌀
        (0x1F308, "rainbow"),        # 🌈
        (0x2602, "umbrella2"),       # ☂️
        (0x2614, "umbrella"),        # ☔
        (0x2604, "comet"),           # ☄️
        (0x1F4A7, "droplet"),        # 💧
        (0x1F30A, "ocean"),          # 🌊
        (0x1F514, "bell"),           # 🔔
        (0x1F515, "no_bell"),        # 🔕
        (0x1F3B5, "music"),          # 🎵
        (0x1F3B6, "notes"),          # 🎶
        (0x1F399, "studio_mic"),     # 🎙️
        (0x1F39A, "level"),          # 🎚️
        (0x1F39B, "knobs"),          # 🎛️
        (0x1F4FA, "tv"),             # 📺
        (0x1F507, "mute"),           # 🔇
        (0x1F508, "quiet"),          # 🔈
        (0x1F509, "sound"),          # 🔉
        (0x1F50A, "loud"),           # 🔊
        (0x1F4E3, "mega"),           # 📣
        (0x1F4E2, "loudspeaker"),    # 📢
        (0x1F50B, "battery"),        # 🔋
        (0x1F50C, "plug"),           # 🔌
        (0x2757, "exclamation"),     # ❗
        (0x2753, "question"),        # ❓
        (0x2754, "grey_question"),   # ❔
        (0x2755, "grey_excl"),       # ❕
        (0x2049, "interrobang"),     # ⁉️
        (0x203C, "bangbang"),        # ‼️
        (0x1F534, "red_circle"),     # 🔴
        (0x1F7E0, "orange_circle"),  # 🟠
        (0x1F7E1, "yellow_circle"),  # 🟡
        (0x1F7E2, "green_circle"),   # 🟢
        (0x1F535, "blue_circle"),    # 🔵
        (0x1F7E3, "purple_circle"),  # 🟣
        (0x1F7E4, "brown_circle"),   # 🟤
        (0x26AB, "black_circle"),    # ⚫
        (0x26AA, "white_circle"),    # ⚪
        (0x1F7E5, "red_square"),     # 🟥
        (0x1F7E7, "orange_square"),  # 🟧
        (0x1F7E8, "yellow_square"),  # 🟨
        (0x1F7E9, "green_square"),   # 🟩
        (0x1F7E6, "blue_square"),    # 🟦
        (0x1F7EA, "purple_square"),  # 🟪
        (0x1F7EB, "brown_square"),   # 🟫
        (0x2B1B, "black_square"),    # ⬛
        (0x2B1C, "white_square"),    # ⬜
        (0x25FC, "black_medium"),    # ◼️
        (0x25FB, "white_medium"),    # ◻️
        (0x25FE, "black_small"),     # ◾
        (0x25FD, "white_small"),     # ◽
        (0x25AA, "black_tiny"),      # ▪️
        (0x25AB, "white_tiny"),      # ▫️
        (0x1F536, "orange_diamond"), # 🔶
        (0x1F537, "blue_diamond"),   # 🔷
        (0x1F538, "small_orange"),   # 🔸
        (0x1F539, "small_blue"),     # 🔹
        (0x1F53A, "red_triangle"),   # 🔺
        (0x1F53B, "red_triangle2"),  # 🔻
        (0x1F4A0, "diamond_shape"),  # 💠
        (0x1F518, "radio_button"),   # 🔘
        (0x1F532, "black_button"),   # 🔲
        (0x1F533, "white_button"),   # 🔳
        (0x26D4, "no_entry"),        # ⛔
        (0x1F6AB, "no_entry2"),      # 🚫
        (0x1F6B3, "no_bikes"),       # 🚳
        (0x1F6AD, "no_smoking"),     # 🚭
        (0x1F6AF, "no_litter"),      # 🚯
        (0x1F6B1, "no_water"),       # 🚱
        (0x1F6B7, "no_pedestrians"), # 🚷
        (0x1F4F5, "no_phones"),      # 📵
        (0x1F51E, "underage"),       # 🔞
        (0x2622, "radioactive"),     # ☢️
        (0x2623, "biohazard"),       # ☣️
        (0x2B06, "arrow_up"),        # ⬆️
        (0x2197, "arrow_upper_right"),# ↗️
        (0x27A1, "arrow_right"),     # ➡️
        (0x2198, "arrow_lower_right"),# ↘️
        (0x2B07, "arrow_down"),      # ⬇️
        (0x2199, "arrow_lower_left"),# ↙️
        (0x2B05, "arrow_left"),      # ⬅️
        (0x2196, "arrow_upper_left"),# ↖️
        (0x2195, "arrow_up_down"),   # ↕️
        (0x2194, "arrow_left_right"),# ↔️
        (0x21A9, "leftwards"),       # ↩️
        (0x21AA, "rightwards"),      # ↪️
        (0x2934, "arrow_heading_up"),# ⤴️
        (0x2935, "arrow_heading_down"),# ⤵️
        (0x1F503, "clockwise"),      # 🔃
        (0x1F504, "counterclockwise"),# 🔄
        (0x1F519, "back"),           # 🔙
        (0x1F51A, "end"),            # 🔚
        (0x1F51B, "on"),             # 🔛
        (0x1F51C, "soon"),           # 🔜
        (0x1F51D, "top"),            # 🔝
        (0x1F6D0, "place_of_worship"),# 🛐
        (0x269B, "atom"),            # ⚛️
        (0x1F549, "om"),             # 🕉️
        (0x2721, "star_of_david"),   # ✡️
        (0x2638, "wheel"),           # ☸️
        (0x262F, "yin_yang"),        # ☯️
        (0x271D, "cross2"),          # ✝️
        (0x2626, "orthodox"),        # ☦️
        (0x262A, "star_crescent"),   # ☪️
        (0x262E, "peace"),           # ☮️
        (0x1F54E, "menorah"),        # 🕎
        (0x1F52F, "six_star"),       # 🔯
        (0x2648, "aries"),           # ♈
        (0x2649, "taurus"),          # ♉
        (0x264A, "gemini"),          # ♊
        (0x264B, "cancer"),          # ♋
        (0x264C, "leo"),             # ♌
        (0x264D, "virgo"),           # ♍
        (0x264E, "libra"),           # ♎
        (0x264F, "scorpio"),         # ♏
        (0x2650, "sagittarius"),     # ♐
        (0x2651, "capricorn"),       # ♑
        (0x2652, "aquarius"),        # ♒
        (0x2653, "pisces"),          # ♓
        (0x26CE, "ophiuchus"),       # ⛎
        (0x1F500, "shuffle"),        # 🔀
        (0x1F501, "repeat"),         # 🔁
        (0x1F502, "repeat_one"),     # 🔂
        (0x25B6, "play"),            # ▶️
        (0x23E9, "fast_forward"),    # ⏩
        (0x23ED, "next_track"),      # ⏭️
        (0x23EF, "play_pause"),      # ⏯️
        (0x25C0, "reverse"),         # ◀️
        (0x23EA, "rewind"),          # ⏪
        (0x23EE, "prev_track"),      # ⏮️
        (0x1F53C, "up_button"),      # 🔼
        (0x23EB, "fast_up"),         # ⏫
        (0x1F53D, "down_button"),    # 🔽
        (0x23EC, "fast_down"),       # ⏬
        (0x23F8, "pause"),           # ⏸️
        (0x23F9, "stop"),            # ⏹️
        (0x23FA, "record"),          # ⏺️
        (0x23CF, "eject"),           # ⏏️
        (0x1F3A6, "cinema"),         # 🎦
        (0x1F505, "low_bright"),     # 🔅
        (0x1F506, "high_bright"),    # 🔆
        (0x1F4F6, "signal"),         # 📶
        (0x1F4F3, "vibration"),      # 📳
        (0x1F4F4, "phone_off"),      # 📴
        (0x2640, "female"),          # ♀️
        (0x2642, "male"),            # ♂️
        (0x26A7, "transgender"),     # ⚧️
        (0x2716, "heavy_mult"),      # ✖️
        (0x2795, "heavy_plus"),      # ➕
        (0x2796, "heavy_minus"),     # ➖
        (0x2797, "heavy_div"),       # ➗
        (0x267E, "infinity"),        # ♾️
        (0x1F4B2, "heavy_dollar"),   # 💲
        (0x1F4B1, "currency"),       # 💱
        (0x00A9, "copyright"),       # ©️
        (0x00AE, "registered"),      # ®️
        (0x2122, "tm"),              # ™️
        (0x0030, "zero"),            # 0️⃣
        (0x0031, "one"),             # 1️⃣
        (0x0032, "two"),             # 2️⃣
        (0x0033, "three"),           # 3️⃣
        (0x0034, "four"),            # 4️⃣
        (0x0035, "five"),            # 5️⃣
        (0x0036, "six"),             # 6️⃣
        (0x0037, "seven"),           # 7️⃣
        (0x0038, "eight"),           # 8️⃣
        (0x0039, "nine"),            # 9️⃣
        (0x1F51F, "ten"),            # 🔟
        (0x1F520, "abc_upper"),      # 🔠
        (0x1F521, "abc_lower"),      # 🔡
        (0x1F522, "numbers"),        # 🔢
        (0x1F523, "symbols"),        # 🔣
        (0x1F524, "abc"),            # 🔤
        (0x1F170, "a_button"),       # 🅰️
        (0x1F18E, "ab_button"),      # 🆎
        (0x1F171, "b_button"),       # 🅱️
        (0x1F191, "cl"),             # 🆑
        (0x1F192, "cool"),           # 🆒
        (0x1F193, "free"),           # 🆓
        (0x2139, "info"),            # ℹ️
        (0x1F194, "id"),             # 🆔
        (0x24C2, "m"),               # Ⓜ️
        (0x1F195, "new"),            # 🆕
        (0x1F196, "ng"),             # 🆖
        (0x1F17E, "o_button"),       # 🅾️
        (0x1F197, "ok"),             # 🆗
        (0x1F17F, "parking"),        # 🅿️
        (0x1F198, "sos"),            # 🆘
        (0x1F199, "up"),             # 🆙
        (0x1F19A, "vs"),             # 🆚
        (0x1F201, "koko"),           # 🈁
        (0x1F202, "sa"),             # 🈂️
        (0x1F237, "monthly"),        # 🈷️
        (0x1F236, "u6709"),          # 🈶
        (0x1F22F, "u6307"),          # 🈯
        (0x1F250, "u5272"),          # 🉐
        (0x1F239, "u5408"),          # 🈹
        (0x1F21A, "u7121"),          # 🈚
        (0x1F232, "u7981"),          # 🈲
        (0x1F251, "u7a7a"),          # 🉑
        (0x1F238, "u7533"),          # 🈸
        (0x1F234, "u5408_2"),        # 🈴
        (0x1F233, "u7a7a_2"),        # 🈳
        (0x3297, "circled_ideograph"),# ㊗️
        (0x3299, "circled_secret"),  # ㊙️
        (0x1F23A, "u55b6"),          # 🈺
        (0x1F235, "u6e80"),          # 🈵
    ],
    "FLAGS": [
        (0x1F3C1, "checkered"),      # 🏁
        (0x1F6A9, "triangular"),     # 🚩
        (0x1F38C, "crossed_flags"),   # 🎌
        (0x1F3F4, "black_flag"),     # 🏴
        (0x1F3F3, "white_flag"),     # 🏳️
        (0x1F3F3, "rainbow_flag"),   # 🏳️‍🌈 (note: this needs special handling)
        (0x1F3F4, "pirate"),         # 🏴‍☠️
        # Common country flags (two-letter codes convert to regional indicators)
        # These use regional indicator symbols A-Z (0x1F1E6-0x1F1FF)
    ],
}

# Total count
total = sum(len(v) for v in EMOJI_DATA.values())
print(f"// Total emoji: {total}", file=sys.stderr)


def ensure_cache_dir():
    """Create cache directory if it doesn't exist"""
    if not os.path.exists(CACHE_DIR):
        os.makedirs(CACHE_DIR)


def codepoint_to_twemoji_filename(codepoint):
    """Convert codepoint to Twemoji filename format"""
    return f"{codepoint:x}.png"


def download_twemoji(codepoint):
    """Download a Twemoji PNG file, returns image bytes or None"""
    filename = codepoint_to_twemoji_filename(codepoint)
    cache_path = os.path.join(CACHE_DIR, filename)

    # Check cache first
    if os.path.exists(cache_path):
        with open(cache_path, 'rb') as f:
            return f.read()

    # Download from Twemoji
    url = f"{TWEMOJI_BASE}/{filename}"

    try:
        # Create SSL context that doesn't verify certificates (for macOS)
        ctx = ssl.create_default_context()
        ctx.check_hostname = False
        ctx.verify_mode = ssl.CERT_NONE

        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        with urllib.request.urlopen(req, timeout=10, context=ctx) as response:
            data = response.read()

        # Cache the file
        ensure_cache_dir()
        with open(cache_path, 'wb') as f:
            f.write(data)

        return data
    except (urllib.error.URLError, urllib.error.HTTPError) as e:
        print(f"  Failed to download {filename}: {e}", file=sys.stderr)
        return None


def rgb_to_rgb565(r, g, b):
    """Convert RGB888 to RGB565"""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def process_twemoji_to_rgb565(png_data, size=12):
    """Convert Twemoji PNG to 12x12 RGB565 array"""
    try:
        img = Image.open(io.BytesIO(png_data))

        # Convert to RGBA if needed
        if img.mode != 'RGBA':
            img = img.convert('RGBA')

        # Resize to target size with high-quality resampling
        img = img.resize((size, size), Image.Resampling.LANCZOS)

        # Create black background for transparency
        background = Image.new('RGB', (size, size), (0, 0, 0))

        # Composite emoji onto background
        # Split alpha channel and use it as mask
        if img.mode == 'RGBA':
            r, g, b, a = img.split()
            rgb_img = Image.merge('RGB', (r, g, b))
            background.paste(rgb_img, mask=a)
        else:
            background.paste(img)

        # Convert to RGB565
        pixels = list(background.getdata())
        rgb565_data = []
        for r, g, b in pixels:
            rgb565_data.append(rgb_to_rgb565(r, g, b))

        return rgb565_data
    except Exception as e:
        print(f"  Error processing image: {e}", file=sys.stderr)
        return None


def generate_placeholder(size=12):
    """Generate a placeholder for missing emoji (question mark pattern)"""
    # Create a simple "?" pattern in yellow on black
    data = []
    pattern = [
        "000111111000",
        "001111111100",
        "011100001110",
        "011100001110",
        "000000011110",
        "000000111100",
        "000001111000",
        "000001110000",
        "000001110000",
        "000000000000",
        "000001110000",
        "000001110000",
    ]

    yellow = rgb_to_rgb565(255, 215, 0)
    black = rgb_to_rgb565(0, 0, 0)

    for row in pattern:
        for c in row:
            data.append(yellow if c == '1' else black)

    return data


def main():
    print("Generating emoji data with Twemoji...", file=sys.stderr)
    ensure_cache_dir()

    # Header
    print("/**")
    print(" * MeshBerry Emoji Bitmap Data (Auto-Generated from Twemoji)")
    print(" * ")
    print(" * SPDX-License-Identifier: GPL-3.0-or-later")
    print(" * Copyright (C) 2026 NodakMesh (nodakmesh.org)")
    print(" * ")
    print(" * Twemoji graphics licensed under CC-BY 4.0")
    print(" * https://github.com/twitter/twemoji")
    print(" * ")
    print(f" * Total: {total} emoji as 12x12 RGB565 bitmaps")
    print(" */")
    print()
    print("#ifndef MESHBERRY_EMOJI_DATA_H")
    print("#define MESHBERRY_EMOJI_DATA_H")
    print()
    print("#include <Arduino.h>")
    print("#include \"Emoji.h\"")
    print()

    # Generate bitmaps
    all_entries = []
    success_count = 0
    fail_count = 0

    for category, emojis in EMOJI_DATA.items():
        print(f"// ============ {category} ({len(emojis)} emoji) ============")
        print()

        for codepoint, shortcode in emojis:
            var_name = f"EMOJI_BMP_{shortcode.upper()}"

            # Try to download and process Twemoji
            print(f"  Processing {chr(codepoint) if codepoint < 0x10000 else ''} {shortcode}...", file=sys.stderr)

            png_data = download_twemoji(codepoint)
            if png_data:
                data = process_twemoji_to_rgb565(png_data)
                if data:
                    success_count += 1
                else:
                    data = generate_placeholder()
                    fail_count += 1
            else:
                data = generate_placeholder()
                fail_count += 1

            # Output as PROGMEM array
            print(f"static const uint16_t {var_name}[144] PROGMEM = {{")
            for i in range(0, 144, 12):
                row = data[i:i + 12]
                hex_row = ", ".join(f"0x{v:04X}" for v in row)
                comma = "," if i + 12 < 144 else ""
                print(f"    {hex_row}{comma}")
            print("};")
            print()

            all_entries.append((codepoint, shortcode, var_name, category))

            # Small delay to avoid hammering the server
            time.sleep(0.05)

    # Generate the emoji table
    print("// ============ EMOJI TABLE ============")
    print()
    print(f"const int EMOJI_COUNT = {len(all_entries)};")
    print()
    print("const EmojiEntry EMOJI_TABLE[EMOJI_COUNT] PROGMEM = {")

    for codepoint, shortcode, var_name, category in all_entries:
        cat_enum = f"EmojiCategory::{category}"
        print(f'    {{ 0x{codepoint:05X}, "{shortcode}", {var_name}, {cat_enum} }},')

    print("};")
    print()
    print("#endif // MESHBERRY_EMOJI_DATA_H")

    print(f"\nDone! Success: {success_count}, Failed: {fail_count}", file=sys.stderr)


if __name__ == "__main__":
    main()

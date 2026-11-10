# Controls and features

## Camera

Choose Standard, Close or Room, then adjust individual shots. There are 39 angles,
each with its own field of view, movement and frequency. Three slots store custom
presets. Disabled shots and shots with zero frequency stay excluded during opening,
obstruction recovery and menu resume.

Change angles after a number of dialogue lines, on a timer, or with both.
Timers have separate settings for speaking and choosing a reply.
Camera effects include push, pull, crane, tilt, drift, orbit, slide and zoom.

**Reaction Shots** (off by default) occasionally play one of the other person's
lines on you. After every N of their lines (Lines Before A Reaction), the
Reaction Chance is rolled; on a hit, their next line is shown on you listening,
then the camera goes back. Short lines and full-intensity lines are skipped,
and a framing chosen with the hotkey takes priority.

Both line counts, for angle changes and for reactions, start again with every
reply, so each answer to a topic you pick is counted on its own.

**Stay On You After You Speak** (under Holds) sets how long the camera stays on
you after a voiced player line ends before it cuts to them. 0 cuts immediately.
It only covers the silence before their reply: once they start talking the
camera goes to them. Under Dragonborn ReVoiced the reply follows your line at
once unless ReVoiced's own post-line delay is set.
Stay On You After You Pick is the equivalent for unvoiced lines, timed from
the start of their reply.

## Settings

Open **Cinematic Conversation Camera** in SKSE Menu Framework. Pages cover
Presets, Camera, Shots, Screen, Faces, Keys and About. There is no MCM.

Defaults are in `Data/SKSE/Plugins/SD.ini`. Menu changes save to `SD_user.ini`
and override the corresponding defaults. Updates replace SD.ini; keep SD_user.ini.
Settings marked `(restart)` need a game restart.

| Setting | Default |
|---|---|
| Preset | Close |
| Direct the Camera | On |
| Return to First Person Afterwards | On |
| Keep Subject Visible | Off |
| True 180 Rule | Off |
| Close-Ups Follow The Face | On |
| Reaction Shots | Off; 50% after every 3 lines |
| Stay On You After You Speak | 0 s |
| First-Person Fallback | On, with subject protection |
| Per Line Angle Change | Every 3–6 eligible lines |
| Ignore Short Lines | On |
| Timers | Off |
| Black Bars | On, 12% |
| Fade Out Dialogue | On |
| Hide NPC Name | On |
| Fade After PC Line | On, 1.5 s delay, 2 s fade |
| Subtitles In The Black Bar | Off |
| Hold On Their Answer (persuasion) | On |
| Film When You Stand Still | On; within 600 units, after 1.5 s |
| Film Their Conversation key | Unassigned |
| Lip Sync Fallback | On |
| Responsive Expressions | On |
| Your Gestures Start With Your Line | On |
| They Stop Working To Talk | On |
| You Turn To Face Them | On |
| Both Of Them In One Shot | On; 35%, up to 3 lines, within 220 units |
| Lighting | Off |

## Presets

The Presets page has three built-in looks (Standard, Close, Room), any
installed presets, three slots of your own, and an Export section. Ticking a
preset rewrites the Shots page; everything stays editable afterwards.

**Installing presets.** Preset files are `.json` files in
`Data/SKSE/Plugins/SceneDirector/Presets/`, so a preset mod is just a mod
with a file in that folder. Install as many as you like; each one is listed
under Installed Presets. A file can hold one preset or a list of them.

**Sharing your own.** Fill in a name (author and description are optional)
and press Export Current Settings. The file is saved as
`SKSE/Plugins/SceneDirector/Presets/<name>.json`; under Mod Organizer it lands
in Overwrite, so move it into a mod to keep it. To share it, upload that mod.

A preset file looks like this (this is Close, exported):

```json
{
    "name": "Tavern Talk",
    "author": "Someone",
    "description": "Tight on faces, slow cuts.",

    "cutting": {
        "changeAnglePerLine": true,
        "minimumLines": 3,
        "maximumLines": 6,
        "ignoreShortLines": true,
        "timerWhileTalking": false,
        "timerWhileChoosing": false,
        "changeAfter": 9.0,
        "shortestHold": 2.4
    },

    "angles": {
        "OverPlayerShoulder": { "frequency": 34, "fov": 40, "effect": "None", "light": "soft" },
        "CloseUp":            { "frequency": 36, "fov": 50, "effect": "Push in", "amount": 40, "duration": 8.0, "light": "soft" },
        "ExtremeClose":       { "frequency": 75, "fov": 40, "effect": "Zoom in", "amount": 40, "duration": 9.0, "light": "hard" },
        "ClosePlayer":        { "frequency": 30, "fov": 40, "effect": "Zoom out", "amount": 20, "duration": 5.0, "light": "soft" },
        "ExtremeClosePlayer": { "frequency": 75, "fov": 35, "effect": "Push in", "amount": 75, "duration": 7.0, "light": "hard" },
        "Distant":            { "frequency": 10, "fov": 40, "effect": "Pull out", "amount": 55, "duration": 8.0, "light": "off" }
    }
}
```

The settings are the ones on the Camera and Shots pages, in seconds where the
menu shows seconds:

- `cutting`: Change Angle Per Line, Minimum and Maximum Lines (1-20), Ignore
  Short Lines, Timer While Talking, Timer While Choosing, Change After (1-20 s)
  and Shortest Hold (0.3-9 s).
- Each angle: Frequency (0-100), FOV (20-150), Effect, Amount (0-100),
  Duration (0.3-9 s) and Light.
- Effects: None, Push in, Pull out, Crane up, Crane down, Tilt up, Tilt down,
  Drift, Zoom in, Zoom out, Orbit left, Orbit right, Slide left, Slide right.
- Lights: off, natural, soft, hard, edge. Only used with `[Lighting] bPerShot=1`.

The angles listed are the ones in use; every other angle is off. An angle can
be listed with just `{}` to use its own defaults, and anything else left out
gets the angle's default too (the cutting gets Close's). Spelling is forgiving
about case and spaces, `//` comments are allowed, and anything the mod doesn't
recognise is skipped and shown under the preset on the Presets page. A file
with a syntax error is listed with the line and column of the mistake.

Angles are named by their settings key, because the Shots page uses the same
name on both sides of the conversation:

| Of them | | Of you | | Of both, or the room | |
|---|---|---|---|---|---|
| OverPlayerShoulder | Over Your Shoulder | OverNpcShoulder | Over Their Shoulder | TwoShot | Both Of You |
| OverPlayerShoulderLow | (Low) | OverNpcShoulderLow | (Low) | Profile | Both Of You (Side On) |
| OverPlayerShoulderHigh | (High) | OverNpcShoulderHigh | (High) | Wide | Wide |
| OverPlayerShoulderWide | (Wide) | OverNpcShoulderWide | (Wide) | Master | The Whole Room |
| DirtyNpc | (Tight) | DirtyPlayer | (Tight) | GroundLevel | From The Floor |
| ThreeQuarterNpc | Three Quarters | ThreeQuarterPlayer | Three Quarters | Distant | From Far Off |
| CloseUp | Close Up | ClosePlayer | Close Up | DistantLow | From Far Off (Low) |
| ExtremeClose | Extreme Close Up | ExtremeClosePlayer | Extreme Close Up | | |
| CloseLow | Close Up (Low) | MediumPlayer | Head And Shoulders | | |
| CloseHigh | Close Up (High) | PlayerProfile | Side On | | |
| CloseProfile | Close Up (Side On) | PlayerLow | From Below | | |
| CloseWide | Close Up (Wide) | HighAngle | From Above | | |
| MediumNpc | Head And Shoulders | LongPlayer | Full Figure | | |
| MediumProfile | Head And Shoulders (Side On) | PlayerOverhead | From High Above | | |
| LowAngle | From Below | | | | |
| LowProfile | From Below (Side On) | | | | |
| LongNpc | Full Figure | | | | |
| Overhead | From High Above | | | | |

Without SKSE Menu Framework, set `[Presets] sApply=` in SD_user.ini to a
built-in preset or an installed preset's name. It is applied on the next
start and then cleared.

### Presets by situation

A preset can say when it should be used, and then it switches itself on for
those conversations: an inn at night, a follower, a conversation outdoors in
the rain. When the conversation ends, your own settings come back. Nothing it
does is saved, so the menu always shows your own settings between
conversations.

The quickest way to make one is the Export section: set up the camera the way
you want it, pick a place, time, weather or person under **Use It By
Itself**, and export. **Who** also offers whoever you last spoke to by name.
Installed presets that switch themselves on show an **Automatic** line under
their description, and **Switch Presets By Situation** turns all of them off
at once (`[Presets] bSituational`). Ticking one still applies it to your own
settings, like any other preset.

In a file, add a `when` block. Every condition in it has to hold, and a list
holds if any entry in it does:

```json
{
    "name": "Tavern Nights",
    "use": "Close",
    "when": { "location": ["inn", "The Winking Skeever"], "time": "night" },
    "unless": { "who": "follower" },
    "cutting": { "minimumLines": 4, "maximumLines": 7 }
}
```

| Condition | What it checks | Examples |
|---|---|---|
| `inside` / `outside` | Whether you're in an interior | `true` |
| `location` | Where you are, and every place that's inside of. A kind of place, a location's name, a location keyword or a form | `"inn"`, `"Whiterun"`, `"LocTypeDwarvenAutomatons"` |
| `time` | The hour in game | `"day"` (6-20), `"night"` (20-6), `"morning"`, `"afternoon"`, `"evening"`, `"21:30-2"` |
| `weather` | Outdoors only; nothing matches inside | `"clear"`, `"cloudy"`, `"rain"`, `"snow"` |
| `who` | What the person you're talking to is | `"follower"`, `"merchant"`, `"innkeeper"`, `"guard"`, `"child"`, `"creature"` |
| `npc` | A particular person, by name or form | `"Lydia"`, `"Skyrim.esm\|A2C8E"` |
| `race` | Their race, by name, editor ID or form | `"Khajiit"`, `"Argonian"` |
| `faction` | A faction they're in, by name or form | `"Skyrim.esm\|48362"` (the Companions) |
| `keyword` | A keyword on them or their race | `"ActorTypeNPC"`, `"Vampire"` |
| `conversation` | Yours, or one you're filming with the scene key | `"yours"`, `"overheard"` |
| `chance` | How often it applies when everything else fits | `50` |

Kinds of place: inn, city, town, settlement, home, dungeon, castle, temple,
shop, jail, guild, farm, mine, camp, barracks, cemetery, ship. A city counts
from anywhere inside it, so `"city"` also holds in that city's inn.

Forms are written as `Plugin.esp|ID` (or `ID~Plugin.esp`); the load order
byte copied from xEdit is ignored. Names match ignoring case and spaces.

- `unless` takes the same conditions. If they all hold, the preset sits this
  one out.
- When more than one preset fits, the highest `priority` wins (default 0),
  then the one with more conditions, then the first by name. A `when` that is
  just `{}` fits everything, which is only useful with `chance` or a low
  priority.
- `use` starts from another preset, built-in (`"Standard"`, `"Close"`,
  `"Room"`) or installed, so a file can change just a few things. `cutting`
  then only changes what it lists. If `angles` is there it replaces the angle
  list, and each angle the other preset also uses keeps its settings for
  anything left out.
- The log says which preset was chosen for each conversation and why, and
  when one fits but loses its `chance` roll.

## The 180-degree rule

**True 180 Rule** keeps the camera on one side of the conversation. You are
filmed over one shoulder and the other person over the opposite one, so you
face each other across every cut. Off, both over-the-shoulders use the same
shoulder and each reverse crosses the line. Set `[Direction] bTrue180=1`, or
enable it under Camera > Framing. It also turns on Never Cross The Eyeline,
which on its own only stops a single angle swinging across the line.

## Close-ups

**Close-Ups Follow The Face** (Camera > Framing, `[Direction] bFollowFace`)
keeps close-ups anchored to the person and frames the face their head bone is
actually carrying. Someone leaning over a workbench is framed on their lowered
face rather than on where their head was when the conversation started, and the
camera turns up to 35 degrees toward the way the face points without crossing
the line. The angle is chosen when the shot cuts and then held. Humanoids only.

## Persuasion

**Hold On Their Answer** (Camera > Persuasion, `[Direction] bPersuasionBeat`,
default on). When you persuade, intimidate or bribe, their answer cuts to a
close-up of them, held for the whole reply while the camera pushes in slowly.
If the camera has only just arrived on them, it stays on that shot instead of
cutting again. Speech checks are recognised from the reply's own conditions,
so this works in every language; the English "(Persuade)" style tags are a
second opinion for modded checks.

## Other people's conversations

Two NPCs talking to each other can be filmed like a conversation of your own.
The second NPC takes your place in every shot, so over-the-shoulders, reverses
and the 180 rule all work. Their subtitles stay on screen; nothing of yours is
touched, and the scene's own voices and faces are left to the game. NPCs
sometimes talk across a room; while the two stand more than 300 units apart
the over-the-shoulders are skipped, since the camera would have to stand
behind the far one.

Nothing films other people's conversations until you bind the key or turn
on the automatic mode below; both ship off. The game's subtitle settings
don't matter: lines are caught as the game starts them, subtitled or not.

The HUD and other mods' widgets are hidden as in your own conversations.
Interface a mod draws itself rather than as a game menu, such as Custom
Markers' mini map, can't be hidden that way; mods that hide for Photo Mode
do hide, because Photo Mode's `PhotoMode_IsActive` is raised while filming
and put back afterwards.

- **Film Their Conversation** (Keys page, `[Direction] iKeyFilmScene`):
  press it near a conversation in front of you to film it, press again to stop.
  It also works on one NPC talking at you or at a crowd.
- **Film When You Stand Still** (Camera > Other People's Conversations,
  `bFilmScenesAuto`, on by default since 1.5.5): stand still within `iSceneRange` units
  of two NPCs talking to each other, with them near the middle of your view,
  for `iSceneWait` hundredths of a second, and filming starts on its own. It
  has to be a real conversation: the two of them in the same scene (how the
  game plays authored exchanges and follower banter), or a quick back-and-forth
  between them. A line said to you — a greeting in passing, a follower's aside,
  a merchant calling out — never starts it, unless it is part of a scene the
  game is playing with a second voice in it, as when several people brief you
  at once.
- **Fit Still Shots To Their Movement** (Camera > Other People's
  Conversations, `bFitShotsToMovement`, off by default): the camera never
  follows anyone. At each cut it works out everywhere the person in the shot
  is about to be, and stands as close as it can while all of it stays in
  frame; then neither the camera nor its aim moves. Someone shifting about on
  the spot keeps the shot close, often exactly as designed. Someone about to
  cross the room takes it further back, or round to an angle the walk fits
  better. It never comes closer than the shot itself would.

  Where they'll be comes from where they've been over the last few seconds,
  the scene's next move for them (read before they take a step, including a
  walk queued for right after their line), their AI package, and how they're
  already moving. A package that only has them talk to or watch somebody is
  not a walk, and a walk a package promises that hasn't started a second and
  a half later stops counting until they move. If they leave the frame
  anyway, walk into the lens or go behind something, it cuts to a new angle.

  The forecast is written to `SceneDirector.log` with this off as well, so you
  can see what it would do first: a `Forecast for …` line at each cut where
  someone is moving, naming where the prediction came from and the frame it
  would use, and a `Walk check` line when the shot ends comparing it with how
  far they actually went.
- **Both Of Them In One Shot** (Camera > Other People's Conversations,
  `bTogetherShot`, on by default): when the two being filmed stand within
  `iTogetherReach` units of each other, a change of speaker has an
  `iTogetherChance` percent chance of cutting to one shot of both of them
  instead of to the new speaker. That shot then stays up for two to
  `iTogetherLines` lines of their exchange, whoever is talking, and the line
  after its last cuts back to the speaker. It ends early if someone else joins
  in, someone walks out of it or changes posture, or you press a camera key,
  and lets go at the next line once it has been up for 20 seconds. There are at
  least three changes of speaker between two of them. It uses Both Of You and
  Both Of You (Side On) from the Shots page, whichever are on, and needs
  Keep Subject Visible off. The log names each one (`Two-shot: …`).

Moving hands the camera back straight away, and the pair you walked out of is
left alone for a while. Filming also stops when they go quiet, walk off, start
fighting, when a menu opens, or when you start a conversation of your own. In a
scene the game is playing, everybody in it is filmed as they speak.

When a scene hands you your turn — "Up through the tower, let's go!", then the
same few lines over and over until you move — the camera lets go a second after
the last real line. This is read from the scene itself, not the words: the
game marks the lines it repeats while it waits. Those repeats never start
filming on their own; the key still films them if you ask.

## Obstructions

**Keep Subject Visible** checks the face from the camera position and looks for
a clear enabled shot. Set `[Direction] bKeepSubjectVisible=1` and
`bHoldPlacement=0`, or enable it under Camera > Framing.

**First-Person Fallback** keeps dialogue running in first person when no suitable
shot is available. Cinematic coverage returns after the view stays clear.
With fallback off, the current enabled shot may remain obstructed. If no valid
pose remains, the normal game camera takes over.

**Ignore Obstructions Mid-Shot** checks placement when a shot starts, then holds
it. Moving characters and objects can cross the frame; a moving conversation
can clip. It is mutually exclusive with Keep Subject Visible. If both INI keys
are enabled, `bHoldPlacement` takes priority.

Obstruction adjustments stay within 12 degrees of the intended angle, in
3-degree steps. Selected orbit effects still move normally. Subject protection
checks actors regardless of Avoid Framing Bystanders.

Detection uses collision geometry and approximate actor shapes. Collisionless
objects and transparent foliage may behave differently from their appearance.
Subject protection still needs in-game testing with SmoothCam and Improved Camera.

## Dialogue and menus

Dialogue options stay visible through the greeting and first choice. After that,
**Fade Out Dialogue** hides them during speech and brings them back for a reply.
Faded options remain clickable. Turn the setting off to keep them visible.

**Fade After PC Line** keeps the list you picked from on screen while your voiced
line plays and has it gone by the time the line ends. When the line's length is
known the fade runs over its last moments; otherwise it starts as the line ends.
ReVoiced reports line endings and skips. DBVO timing uses player sound handles
with a 15-second timeout.

**Subtitles In The Black Bar** (Screen > Black Bars, `[Direction]
bSubtitlesInBar`, off by default) puts the line being spoken in the middle of
the bottom bar and keeps it there as the bar eases in and out. With no bar, or
a bar too thin for the lines, the subtitle goes back over the picture, lifted
just clear of the bar if it would otherwise run into it. While it is on, the
bars are drawn beneath the interface, so menus and notifications draw over
them.

Inventory, barter, crafting and other menus suspend the camera presentation.
Closing the menu resumes the conversation and selects a new shot if needed.

**Voices Up Close** (Screen > Sound, `[Sound] bVoicesUpClose`, on by default)
keeps dialogue at the same volume wherever the camera is. The game plays a
line from the speaker's head and hears it from the camera, so a wide shot
used to make the voice quieter and a reverse shot moved it to the other ear.
With this on, the lines spoken by everyone in the conversation play from just
in front of the camera, like dialogue recorded close to the actor; the rest of
the world is still heard from where the camera stands. A line still playing
when the conversation ends goes back to the speaker.

## Player voice and expressions

DBVO 1, DBVO 2 and Dragonborn ReVoiced are detected automatically. A voice mod
is optional. ReVoiced 1.4.4 or newer reports voice timing directly and supplies
player lip sync. DBVO uses recording lookup and the optional Lip Sync Fallback.

**Responsive Expressions** adds brow and squint movement while speaking and
facial reactions while listening. Player dialogue uses English text cues; NPC
dialogue uses the game's emotion data. Voice tone and sarcasm are not analyzed.
Timing is estimated across phrases, without word-level audio alignment.

Strength is fixed. Profiles can be changed in `Expression.*` INI sections.
Speaking leaves mouth lip sync, blinking and gaze to their existing systems.
Listening expressions yield when the player speaks.

**Your Gestures Start With Your Line** (Faces > Body) sends the PlayDBVOTopic
event when your voiced line starts, so a DBVO player-gesture add-on such as
DBVO Player Gestures Add-On For OAR gestures while you speak. ReVoiced and DBVO 2
do not send that event themselves. It is not sent while DBVO 1 is loaded, since
DBVO 1 sends its own. Gesture mods that use the OAR Dialogue "player has chosen"
condition start when the NPC replies, and this setting does not change them.

**They Stop Working To Talk** (Faces > Body): somebody at a workbench, smelter,
forge, tanning rack, chopping block or similar steps away from it 3 to 5 seconds
after they start talking. Seats, beds, lean spots and quest scenes are left
alone. SceneDirector.log records whether they stood up.

**You Turn To Face Them** (Faces > Body, `[Performance] bTurnToFace`): when a
conversation starts, you turn to face whoever you're talking to, so someone you
spoke to from the side isn't answered over your shoulder. A quarter turn takes
about 0.4 seconds. If they end up more than 30 degrees off to one side during
the conversation, you turn again. You aren't turned while seated, mounted,
using furniture, in the air or knocked down, and if another mod keeps turning
you during the conversation it's left to that mod. When the conversation ends,
a free-look camera is put back looking where it was before, not swung round
with your body.

Eye & Cheek Detail needs a matching regional asset. The tested asset uses the
High Poly Head female mesh and is not bundled. Other heads retain native
brow and squint movement. The expression profile keys are described in SD.ini
under [Performance].

## Compatibility

Keep SmoothCam enabled. The mod takes control during dialogue and returns it
afterward.

For Improved Camera in first person, set `bScripted=0` under `[EVENTS]` in
`SKSE/Plugins/ImprovedCameraSE/Profiles/Default.ini`, or your active profile.
This affects all of Improved Camera's scripted forced-third-person events.
Third-person players do not need the change.

Disable Improved Alternate Conversation Camera, Alternate Conversation Camera,
Switch Camera During Dialogue, and Camera Noise or other camera-shake mods.
Detected conflicts are listed in SceneDirector.log.

Compatible mods include True Directional Movement, No Camera Collision, ENB,
TrueHUD, Compass Navigation Overhaul, moreHUD, SkyHUD, Infinity UI and SkyUI.

## Lighting

Experimental and off by default. Set `[Lighting] bLights=1` to enable it.
There is no lighting page in the panel, and presets leave it off.

## Troubleshooting

Read `Documents/My Games/Skyrim Special Edition/SKSE/SceneDirector.log`.
It records camera cuts, available space, conflicts and conversation setup time.

The mod reuses cached settings while INI files are unchanged. File edits
apply at the next conversation; menu changes invalidate cached values.
In-game timing still needs testing. The reported exit/re-entry dialogue control
lock remains unresolved.

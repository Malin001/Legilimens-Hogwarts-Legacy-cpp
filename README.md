# Legilimens
Legilimens is a tool to help you find your last few missing collectibles in Hogwarts Legacy and finally get that 100% completion. All you need to do is run Legilimens with your save file, and it will analyze and return a list of every collectible that you're missing, including a link to a Youtube video with a timestamp showing its location. Legilimens does **not** edit your save file, it only reads the databases contained within it.

You can find the old (Python) version of this tool [here](https://github.com/Malin001/Legilimens-Hogwarts-Legacy-Collectible-Finder). You can also find this tool on [Nexus](https://www.nexusmods.com/hogwartslegacy/mods/556).

#### What Legilimens can find:
- Field Guide Pages
- Collection Chests
- Demiguise Statues
- Vivarium Chests
- Butterfly Chests
- Daedalian Keys
- Merlin Trials
- Balloon Sets
- Landing Platforms
- Ancient Magic Hotspots
- Infamous Foes
- "Finishing Touch" achievement enemies (EXPERIMENTAL, LIKELY BROKEN)

#### What it *can't* find:
- Items that you are *required* to get during a quest. For example, there are two field guide pages that you are forced to pick up during the History of Magic class quest
- Collectible items that can be bought from vendors
- Collectible items that are rewards for completing quests


## Usage
Simply run `Legilimens.exe` and follow the prompts on screen. Alternatively, you can drag and drop your save file onto `Legilimens.exe`, or run it from the command line as described below.

#### Running from the command line
The first (optional) positional argument is the path to your .sav file. If you don't provide this, Legilimens will prompt you for it when it runs.

You can pass filters to Legilimens to only show certain types of collectibles, using `--filters FILTER1 FILTER2 ...`. The available filters are: ALL, PAGES, CHESTS, DEMIGUISE, DAEDALIAN, MERLIN, ASTRONOMY, LANDING, BALLOONS, HOTSPOT, FOE, ACHIEVEMENTS, TRAITS, CONJURATIONS, WANDS. If you don't provide any filters, Legiliments will prompt you for it when it runs

You can remove the `"Press enter to close this window..."` prompt by passing the argument `--dont-confirm-exit` to Legilimens. This is primarily for anyone that wants to write the output of Legilimens directly to a file.

Some example commands:
- `Legilimens.exe C:\Users\USER\AppData\Local\HogwartsLegacy\Saved\SaveGames\USERID\HL-00-00.sav --filters ALL --dont-confirm-exit > output.txt` will find every collectible and write the output to output.txt
- `Legilimens.exe --filters PAGES DEMIGUISE` will prompt you for your save file, and then print all of the missing collection chests and demiguise statues
- `Legilimens.exe C:\Users\USER\AppData\Local\HogwartsLegacy\Saved\SaveGames\USERID\HL-00-00.sav` will prompt you for which filters to use before outputting all of the corresponding missing collectibles

## FAQ
#### I have 33/34 Field Guide Pages in The Bell Tower Wing, but Legilimens says I've completed them all. Where is it?
- This is a known bug in Hogwarts Legacy that has since been patched, where a certain Bell Tower Wing [flying page](https://youtu.be/KnHZ5gVb_qk&t=104) doesn't count towards your total.
#### Legilimens says that I'm missing something that I've already collected, or doesn't detect all of my missing collectibles, or links to the wrong Youtube video/timestamp, or any other error.
- It's likely an error in my code, so open an [issue](https://github.com/Malin001/Legilimens-Hogwarts-Legacy-cpp/issues) on GitHub, and attach your `.sav` file. I'll do my best to figure out what went wrong and fix it as soon as possible. If you don't have a GitHub account, you can also email your save file to me at Malin4750@gmail.com, or post the output of Legilimens on [Nexus](https://www.nexusmods.com/hogwartslegacy/mods/556). ***If you don't send me either the save file or output, I won't be able to fix the problem for everyone else.***
#### I'm getting the error "SQLite was unable to read parts of the database", which is preventing me from finishing certain collectibles. How can I fix this?
- For some reason, it's possible for parts of your save file to be corrupted and unreadable by SQLite. Unfortunately, I haven't been able to figure out a fix or workaround yet. Sorry.
#### How can I output to a text file?
- First, open the command prompt and navigate to the folder containing `Legilimens.exe` by running `cd path/to/legilimens/folder`. Then, you can run `Legilimens.exe path/to/your/save/file.sav --filters ALL --dont-confirm-exit > output.txt`, and Legilimens will write to the file `output.txt`. See the "Running from the command line" section above for more info.

## Credit
- Thanks to [100% Guides](https://www.youtube.com/@100Guides), [Game Guides Channel](https://www.youtube.com/@GameGuideslolz), [ZaFrostPet](https://www.youtube.com/@ZaFrostPet), and [Lukinator 2321](https://www.youtube.com/@lukinator2321) on Youtube for making the videos that Legilimens links to.
- Also thanks to [ekaomk](https://github.com/ekaomk/Hogwarts-Legacy-Save-Editor), whose Hogwarts Legacy Save Editor code I looked at to learn how to read the save files.
- Thanks to [p-ranav](https://github.com/p-ranav) for creating the [argparse](https://github.com/p-ranav/argparse) and [tabulate](https://github.com/p-ranav/tabulate) C++ libraries, and [aafulei](https://github.com/aafulei) for creating the [color-console](https://github.com/aafulei/color-console) library.
- Thanks to [lillaka](https://www.nexusmods.com/users/2211740) for not only giving me ideas to improve the mod, but actually implementing those ideas and giving me the code

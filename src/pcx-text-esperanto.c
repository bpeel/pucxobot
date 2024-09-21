/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2019  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "pcx-text-esperanto.h"

#include "pcx-text.h"

const char *
pcx_text_esperanto[] = {
        [PCX_TEXT_STRING_LANGUAGE_CODE] =
        "eo",
        [PCX_TEXT_STRING_NAME_COUP] =
        "Puĉo",
        [PCX_TEXT_STRING_NAME_SNITCH] =
        "Perfidulo",
        [PCX_TEXT_STRING_NAME_LOVE] =
        "Amletero",
        [PCX_TEXT_STRING_NAME_WEREWOLF] =
        "Ununokta Homlupo",
        [PCX_TEXT_STRING_NAME_SIX] =
        "6 Prenas",
        [PCX_TEXT_STRING_NAME_FOX] =
        "Vulpo en la Arbaro",
        [PCX_TEXT_STRING_NAME_WORDPARTY] =
        "Vortofesto",
        [PCX_TEXT_STRING_NAME_CHAMELEON] =
        "Kameleono",
        [PCX_TEXT_STRING_NAME_SUPERFIGHT] =
        "Superbatalo",
        [PCX_TEXT_STRING_NAME_ZOMBIE] =
        "Zombiaj Kuboj",
        [PCX_TEXT_STRING_COUP_START_COMMAND] =
        "/pucxo",
        [PCX_TEXT_STRING_COUP_START_COMMAND_DESCRIPTION] =
        "Krei ludon de Puĉo",
        [PCX_TEXT_STRING_SNITCH_START_COMMAND] =
        "/perfidulo",
        [PCX_TEXT_STRING_SNITCH_START_COMMAND_DESCRIPTION] =
        "Krei ludon de Perfidulo",
        [PCX_TEXT_STRING_LOVE_START_COMMAND] =
        "/amletero",
        [PCX_TEXT_STRING_LOVE_START_COMMAND_DESCRIPTION] =
        "Krei ludon de Amletero",
        [PCX_TEXT_STRING_WEREWOLF_START_COMMAND] =
        "/homlupo",
        [PCX_TEXT_STRING_WEREWOLF_START_COMMAND_DESCRIPTION] =
        "Krei ludon de Ununokta Homlupo",
        [PCX_TEXT_STRING_SIX_START_COMMAND] =
        "/ses",
        [PCX_TEXT_STRING_SIX_START_COMMAND_DESCRIPTION] =
        "Krei ludon de 6 Prenas",
        [PCX_TEXT_STRING_FOX_START_COMMAND] =
        "/vulpo",
        [PCX_TEXT_STRING_FOX_START_COMMAND_DESCRIPTION] =
        "Krei ludon de Vulpo en la Arbaro",
        [PCX_TEXT_STRING_WORDPARTY_START_COMMAND] =
        "/vortofesto",
        [PCX_TEXT_STRING_WORDPARTY_START_COMMAND_DESCRIPTION] =
        "Krei ludon de Vortofesto",
        [PCX_TEXT_STRING_CHAMELEON_START_COMMAND] =
        "/kameleono",
        [PCX_TEXT_STRING_CHAMELEON_START_COMMAND_DESCRIPTION] =
        "Krei ludon de Kameleono",
        [PCX_TEXT_STRING_SUPERFIGHT_START_COMMAND] =
        "/superbatalo",
        [PCX_TEXT_STRING_SUPERFIGHT_START_COMMAND_DESCRIPTION] =
        "Krei ludon de Superbatalo",
        [PCX_TEXT_STRING_ZOMBIE_START_COMMAND] =
        "/zombio",
        [PCX_TEXT_STRING_ZOMBIE_START_COMMAND_DESCRIPTION] =
        "Krei ludon de Zombiaj Kuboj",
        [PCX_TEXT_STRING_WHICH_HELP] =
        "Por kiu ludo vi volas helpon?",
        [PCX_TEXT_STRING_TIMEOUT_START] =
        "Neniu aliĝis dum pli ol %i minutoj. La "
        "ludo tuj komenciĝos.",
        [PCX_TEXT_STRING_TIMEOUT_ABANDON] =
        "La ludo estas senaktiva dum pli ol "
        "%i minutoj kaj estos forlasita.",
        [PCX_TEXT_STRING_NEED_PUBLIC_GROUP] =
        "Por ludi ludon, aldonu la roboton al grupo kun viaj amikoj "
        "kaj komencu la ludon tie.",
        [PCX_TEXT_STRING_SEND_PRIVATE_MESSAGE] =
        "Antaŭ ol aliĝi al ludo, bonvolu alklaki @%s kaj sendi mesaĝon por ke "
        "mi povu sendi al vi viajn kartojn en privata mesaĝo. Farinte tion, vi "
        "povos reveni ĉi tien por aliĝi al ludo.",
        [PCX_TEXT_STRING_ALREADY_IN_GAME] =
        "Vi jam estas en ludo",
        [PCX_TEXT_STRING_ALREADY_GAME] =
        "Jam estas ludo en ĉi tiu grupo",
        [PCX_TEXT_STRING_WHICH_GAME] =
        "Kiun ludon vi volas ludi?",
        [PCX_TEXT_STRING_GAME_FULL] =
        "La ludo jam estas plena",
        [PCX_TEXT_STRING_GAME_ALREADY_STARTED] =
        "La ludo jam komenciĝis",
        [PCX_TEXT_STRING_NO_GAME] =
        "Aktuale estas neniu ludo en ĉi tiu grupo.",
        [PCX_TEXT_STRING_CANCELED] =
        "La ludo estas nuligita.",
        [PCX_TEXT_STRING_CANT_CANCEL] =
        "Nur ludantoj en la ludo rajtas nuligi ĝin.",
        [PCX_TEXT_STRING_NAME_FROM_ID] =
        "Sr.%i",
        [PCX_TEXT_STRING_FINAL_CONJUNCTION] =
        " kaj ",
        [PCX_TEXT_STRING_FINAL_DISJUNCTION] =
        " aŭ ",
        [PCX_TEXT_STRING_WELCOME] =
        "Bonvenon. Aliaj ludantoj tajpu /aligxi por aliĝi al la "
        "ludo aŭ tajpu /komenci por komenci ĝin.",
        [PCX_TEXT_STRING_WELCOME_FULL] =
        "Bonvenon. La ludo nun estas plena kaj tuj komenciĝos.",
        [PCX_TEXT_STRING_WELCOME_BUTTONS] =
        "%s aliĝis al la ludo. Vi povas atendi pliajn ludantojn aŭ alklaki la "
        "suban butonon por komenci.",
        [PCX_TEXT_STRING_WELCOME_BUTTONS_TOO_FEW] =
        "%s aliĝis al la ludo. Atendu pliajn ludantojn por komenci la ludon.",
        [PCX_TEXT_STRING_WELCOME_BUTTONS_FULL] =
        "%s aliĝis al la ludo. La ludo nun estas plena kaj tuj komenciĝos.",
        [PCX_TEXT_STRING_PLAYER_LEFT] =
        "%s foriris",
        [PCX_TEXT_STRING_START_BUTTON] =
        "Komenci",
        [PCX_TEXT_STRING_CHOSEN_GAME] =
        "La ludo estas: %s",
        [PCX_TEXT_STRING_CURRENT_PLAYERS] =
        "La aktualaj ludantoj estas:",
        [PCX_TEXT_STRING_JOIN_BEFORE_START] =
        "Aliĝu al la ludo per /aligxi antaŭ ol "
        "komenci ĝin",
        [PCX_TEXT_STRING_NEED_MIN_PLAYERS] =
        "Necesas almenaŭ %i ludantoj por ludi.",
        [PCX_TEXT_STRING_JOIN_COMMAND] =
        "/aligxi",
        [PCX_TEXT_STRING_JOIN_COMMAND_DESCRIPTION] =
        "Aligxi al jam kreita ludo",
        [PCX_TEXT_STRING_START_COMMAND] =
        "/komenci",
        [PCX_TEXT_STRING_START_COMMAND_DESCRIPTION] =
        "Komenci jam kreitan ludon",
        [PCX_TEXT_STRING_CANCEL_COMMAND] =
        "/nuligi",
        [PCX_TEXT_STRING_CANCEL_COMMAND_DESCRIPTION] =
        "Nuligi jam kreitan ludon",
        [PCX_TEXT_STRING_HELP_COMMAND] =
        "/helpo",
        [PCX_TEXT_STRING_HELP_COMMAND_DESCRIPTION] =
        "Montri resumon de la reguloj",
        [PCX_TEXT_STRING_RECEIVED_PRIVATE_MESSAGE] =
        "Dankon pro la mesaĝo. Vi povas nun aliĝi "
        "al ludo en publika grupo.",
        [PCX_TEXT_STRING_CHOOSE_GAME_TYPE] =
        "Bonvolu elekti kiun version de la ludo vi volas ludi.",
        [PCX_TEXT_STRING_GAME_TYPE_CHOSEN] =
        "La elektita versio estas: %s",
        [PCX_TEXT_STRING_GAME_TYPE_ORIGINAL] =
        "Originala",
        [PCX_TEXT_STRING_GAME_TYPE_INSPECTOR] =
        "Inspektisto",
        [PCX_TEXT_STRING_GAME_TYPE_REFORMATION] =
        "Reformacio",
        [PCX_TEXT_STRING_GAME_TYPE_REFORMATION_INSPECTOR] =
        "Reformacio + Inspektisto",
        [PCX_TEXT_STRING_COUP] =
        "Puĉo",
        [PCX_TEXT_STRING_INCOME] =
        "Enspezi",
        [PCX_TEXT_STRING_FOREIGN_AID] =
        "Eksterlanda helpo",
        [PCX_TEXT_STRING_TAX] =
        "Imposto (Duko)",
        [PCX_TEXT_STRING_CONVERT] =
        "Konverti",
        [PCX_TEXT_STRING_EMBEZZLE] =
        "Ŝteli la trezoron",
        [PCX_TEXT_STRING_ASSASSINATE] =
        "Murdi (Murdisto)",
        [PCX_TEXT_STRING_EXCHANGE] =
        "Interŝanĝi (Ambasadoro)",
        [PCX_TEXT_STRING_EXCHANGE_INSPECTOR] =
        "Interŝanĝi (Inspektisto)",
        [PCX_TEXT_STRING_INSPECT] =
        "Inspekti (Inspektisto)",
        [PCX_TEXT_STRING_STEAL] =
        "Ŝteli (Kapitano)",
        [PCX_TEXT_STRING_ACCEPT] =
        "Akcepti",
        [PCX_TEXT_STRING_CHALLENGE] =
        "Defii",
        [PCX_TEXT_STRING_BLOCK] =
        "Bloki",
        [PCX_TEXT_STRING_1_COIN] =
        "1 monero",
        [PCX_TEXT_STRING_PLURAL_COINS] =
        "%i moneroj",
        [PCX_TEXT_STRING_YOUR_CARDS_ARE] =
        "Viaj kartoj estas:",
        [PCX_TEXT_STRING_NOONE] =
        "Neniu",
        [PCX_TEXT_STRING_WON_1] =
        "%s venkis!",
        [PCX_TEXT_STRING_WON_PLURAL] =
        "%s venkis!",
        [PCX_TEXT_STRING_YOUR_GO] =
        "%s, estas via vico, "
        "kion vi volas fari?",
        [PCX_TEXT_STRING_WHICH_CARD_TO_LOSE] =
        "Kiun karton vi volas perdi?",
        [PCX_TEXT_STRING_CHALLENGE_SUCCEEDED] =
        "%s defiis kaj %s ne havis %s kaj %s "
        "perdas karton",
        [PCX_TEXT_STRING_INVERTED_CHALLENGE_SUCCEEDED] =
        "%s defiis kaj %s cedis do %s perdas karton.",
        [PCX_TEXT_STRING_CHALLENGE_FAILED] =
        "%s defiis sed %s ja havis %s. %s "
        "perdas karton kaj %s ricevas novan anstataŭan karton.",
        [PCX_TEXT_STRING_INVERTED_CHALLENGE_FAILED] =
        "%s defiis kaj %s montris %s do %s ŝanĝas siajn kartojn kaj "
        "%s perdas karton.",
        [PCX_TEXT_STRING_CHOOSING_REVEAL] =
        "%s defiis kaj nun %s elektas kiun karton montri.",
        [PCX_TEXT_STRING_CHOOSING_REVEAL_INVERTED] =
        "%s defiis kaj nun %s elektas ĉu cedi.",
        [PCX_TEXT_STRING_ANNOUNCE_CHALLENGE] =
        "%s ne kredas ke vi havas %s.\n"
        "Kiun karton vi volas montri?",
        [PCX_TEXT_STRING_ANNOUNCE_INVERTED_CHALLENGE] =
        "%s kredas ke vi ja havas %s.\n"
        "Ĉu vi volas cedi?",
        [PCX_TEXT_STRING_CONCEDE] =
        "Cedi",
        [PCX_TEXT_STRING_SHOW_CARDS] =
        "Montri kartojn",
        [PCX_TEXT_STRING_NO_CHALLENGE_SO_BLOCK] =
        "Neniu defiis. La ago estis blokita.",
        [PCX_TEXT_STRING_CLAIM_CARDS_TO_BLOCK] =
        "%s pretendas havi %s kaj "
        "blokas.",
        [PCX_TEXT_STRING_DOES_SOMEBODY_CHALLENGE] =
        "Ĉu iu volas defii rin?",
        [PCX_TEXT_STRING_OR_BLOCK_NO_TARGET] =
        "Aŭ ĉu iu volas pretendi havi %s "
        "kaj bloki rin?",
        [PCX_TEXT_STRING_BLOCK_NO_TARGET] =
        "Ĉu iu volas pretendi havi %s kaj "
        "bloki rin?",
        [PCX_TEXT_STRING_OR_BLOCK_OTHER_ALLEGIANCE] =
        "Aŭ ĉu iu de alia partio volas pretendi havi %s "
        "kaj bloki rin?",
        [PCX_TEXT_STRING_BLOCK_OTHER_ALLEGIANCE] =
        "Ĉu iu de alia partio volas pretendi havi %s kaj "
        "bloki rin?",
        [PCX_TEXT_STRING_OR_BLOCK_WITH_TARGET] =
        "Aŭ %s, ĉu vi volas pretendi havi "
        "%s kaj bloki rin?",
        [PCX_TEXT_STRING_BLOCK_WITH_TARGET] =
        "%s, ĉu vi volas pretendi havi %s "
        "kaj bloki rin?",
        [PCX_TEXT_STRING_WHO_TO_COUP] =
        "%s, kiun vi volas mortigi dum la puĉo?",
        [PCX_TEXT_STRING_DOING_COUP] =
        "💣 %s faras puĉon kontraŭ %s",
        [PCX_TEXT_STRING_DOING_INCOME] =
        "💲 %s enspezas 1 moneron",
        [PCX_TEXT_STRING_REALLY_DOING_FOREIGN_AID] =
        "Neniu blokis, %s prenas la 2 monerojn",
        [PCX_TEXT_STRING_DOING_FOREIGN_AID] =
        "💴 %s prenas 2 monerojn per eksterlanda "
        "helpo.",
        [PCX_TEXT_STRING_EMBEZZLING] =
        "💼 %s pretendas ne havi la dukon kaj ŝtelas la monon de la trezorejo.",
        [PCX_TEXT_STRING_REALLY_EMBEZZLING] =
        "Neniu defiis, %s prenas la monon de la trezorejo.",
        [PCX_TEXT_STRING_REALLY_DOING_TAX] =
        "Neniu defiis, %s prenas la 3 monerojn",
        [PCX_TEXT_STRING_DOING_TAX] =
        "💸 %s pretendas havi la dukon kaj prenas "
        "3 monerojn per imposto.",
        [PCX_TEXT_STRING_WHO_TO_CONVERT] =
        "%s, kiun vi volas konverti?",
        [PCX_TEXT_STRING_CONVERTS_SELF] =
        "%s pagas 1 moneron al la trezorejo kaj konvertas sin mem.",
        [PCX_TEXT_STRING_CONVERTS_SOMEONE_ELSE] =
        "%s pagas 2 monerojn al la trezorejo kaj konvertas %s.",
        [PCX_TEXT_STRING_REALLY_DOING_ASSASSINATION] =
        "Neniu blokis aŭ defiis, %s murdas %s",
        [PCX_TEXT_STRING_SELECT_TARGET_ASSASSINATION] =
        "%s, kiun vi volas murdi?",
        [PCX_TEXT_STRING_DOING_ASSASSINATION] =
        "🗡 %s volas murdi %s",
        [PCX_TEXT_STRING_WHICH_CARDS_TO_KEEP] =
        "Kiujn kartojn vi volas konservi?",
        [PCX_TEXT_STRING_REALLY_DOING_EXCHANGE] =
        "Neniu defiis, %s interŝanĝas kartojn",
        [PCX_TEXT_STRING_DOING_EXCHANGE] =
        "🔄 %s pretendas havi la ambasadoron kaj volas "
        "interŝanĝi kartojn",
        [PCX_TEXT_STRING_DOING_EXCHANGE_INSPECTOR] =
        "🔄 %s pretendas havi la inspektiston kaj volas "
        "interŝanĝi kartojn",
        [PCX_TEXT_STRING_REALLY_DOING_INSPECT] =
        "Neniu defiis, %s elektas karton por montri al %s",
        [PCX_TEXT_STRING_SELECT_TARGET_INSPECT] =
        "%s, kies karton vi volas inspekti?",
        [PCX_TEXT_STRING_DOING_INSPECT] =
        "🔍 %s pretendas havi la inspektiston kaj volas inspekti karton de %s",
        [PCX_TEXT_STRING_CHOOSE_CARD_TO_SHOW] =
        "Kiun karton vi montros al %s?",
        [PCX_TEXT_STRING_OTHER_PLAYER_DECIDING_CAN_KEEP] =
        "Nun %s decidas ĉu vi rajtas konservi %s",
        [PCX_TEXT_STRING_SHOWING_CARD] =
        "%s montras al vi %s. Ĉu ri rajtas konservi ĝin?",
        [PCX_TEXT_STRING_YES] =
        "Jes",
        [PCX_TEXT_STRING_NO] =
        "Ne",
        [PCX_TEXT_STRING_ALLOW_KEEP] =
        "%s permesis %s konservi la karton kiun ri montris.",
        [PCX_TEXT_STRING_DONT_ALLOW_KEEP] =
        "%s devigis %s ŝanĝi la karton kiun ri montris.",
        [PCX_TEXT_STRING_REALLY_DOING_STEAL] =
        "Neniu blokis aŭ defiis, %s ŝtelas de %s",
        [PCX_TEXT_STRING_SELECT_TARGET_STEAL] =
        "%s, de kiu vi volas ŝteli?",
        [PCX_TEXT_STRING_DOING_STEAL] =
        "💰 %s volas ŝteli de %s",
        [PCX_TEXT_STRING_CHARACTER_NAME_DUKE] =
        "Duko",
        [PCX_TEXT_STRING_CHARACTER_NAME_ASSASSIN] =
        "Murdisto",
        [PCX_TEXT_STRING_CHARACTER_NAME_CONTESSA] =
        "Grafino",
        [PCX_TEXT_STRING_CHARACTER_NAME_CAPTAIN] =
        "Kapitano",
        [PCX_TEXT_STRING_CHARACTER_NAME_AMBASSADOR] =
        "Ambasadoro",
        [PCX_TEXT_STRING_CHARACTER_NAME_INSPECTOR] =
        "Inspektisto",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_DUKE] =
        "la dukon",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_ASSASSIN] =
        "la murdiston",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CONTESSA] =
        "la grafinon",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CAPTAIN] =
        "la kapitanon",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_AMBASSADOR] =
        "la ambasadoron",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_INSPECTOR] =
        "la inspektiston",
        [PCX_TEXT_STRING_COINS_IN_TREASURY] =
        "Trezorejo: %i",
        [PCX_TEXT_STRING_ROLE_NAME_DRIVER] =
        "Ŝoforo",
        [PCX_TEXT_STRING_ROLE_NAME_LOCKPICK] =
        "Ŝlosisto",
        [PCX_TEXT_STRING_ROLE_NAME_MUSCLE] =
        "Muskolulo",
        [PCX_TEXT_STRING_ROLE_NAME_CON_ARTIST] =
        "Trompartisto",
        [PCX_TEXT_STRING_ROLE_NAME_LOOKOUT] =
        "Gvatisto",
        [PCX_TEXT_STRING_ROLE_NAME_SNITCH] =
        "Perfidulo",
        [PCX_TEXT_STRING_ROUND_NUM] =
        "Raŭndo %i / %i",
        [PCX_TEXT_STRING_CHOOSE_HEIST_DIFFICULTY] =
        "%s, vi estas la rabestro. Kiom da roluloj vi volas por la rabado?",
        [PCX_TEXT_STRING_HEIST_SIZE_CHOSEN] =
        "La rabado bezonos %i rolulojn kiuj estas:",
        [PCX_TEXT_STRING_DISCUSS_HEIST] =
        "Nun vi povas interkonsenti inter vi pri kiun rolon vi aldonos al "
        "la rabado. Kiam vi estas preta, sekrete elektu karton.",
        [PCX_TEXT_STRING_CARDS_CHOSEN] =
        "Ĉiu faris sian elekton! La roluloj estas:",
        [PCX_TEXT_STRING_NEEDED_CARDS_WERE] =
        "La bezonataj kartoj estis:",
        [PCX_TEXT_STRING_YOU_CHOSE] =
        "Vi elektis:",
        [PCX_TEXT_STRING_WHICH_ROLE] =
        "Kiun karton vi volas elekti?",
        [PCX_TEXT_STRING_HEIST_SUCCESS] =
        "La rabado sukcesis! Ĉiu ludanto kiu ne elektis la perfidulon ricevas "
        "%i monerojn.",
        [PCX_TEXT_STRING_HEIST_FAILED] =
        "La rabado fuŝiĝis! Ĉiu kiu ne elektis la perfidulon perdas 1 moneron.",
        [PCX_TEXT_STRING_SNITCH_GAIN_1] =
        "Ĉiu alia gajnas 1 moneron.",
        [PCX_TEXT_STRING_SNITCH_GAIN_PLURAL] =
        "Ĉiu alia gajnas %i monerojn.",
        [PCX_TEXT_STRING_EVERYONE_SNITCHED] =
        "La rabado fuŝiĝis kaj ĉiu perfidis! Neniu gajnas monon.",
        [PCX_TEXT_STRING_NOONE_SNITCHED] =
        "Neniu perfidis.",
        [PCX_TEXT_STRING_1_SNITCH] =
        "1 perfidulo",
        [PCX_TEXT_STRING_PLURAL_SNITCHES] =
        "%i perfiduloj",
        [PCX_TEXT_STRING_GUARD] =
        "Gardisto",
        [PCX_TEXT_STRING_GUARD_OBJECT] =
        "Gardiston",
        [PCX_TEXT_STRING_GUARD_DESCRIPTION] =
        "Nomu karton kiu ne estas gardisto, kaj elektu ludanton. "
        "Se tiu ludanto havas tiun karton, ri perdas la raŭndon.",
        [PCX_TEXT_STRING_SPY] =
        "Spiono",
        [PCX_TEXT_STRING_SPY_OBJECT] =
        "Spionon",
        [PCX_TEXT_STRING_SPY_DESCRIPTION] =
        "Rigardu la manon de alia ludanto.",
        [PCX_TEXT_STRING_BARON] =
        "Barono",
        [PCX_TEXT_STRING_BARON_OBJECT] =
        "Baronon",
        [PCX_TEXT_STRING_BARON_DESCRIPTION] =
        "Sekrete komparu manojn kun alia ludanto. Tiu de vi ambaŭ, "
        "kiu havas la malplej valoran karton, perdas la raŭndon.",
        [PCX_TEXT_STRING_HANDMAID] =
        "Servistino",
        [PCX_TEXT_STRING_HANDMAID_OBJECT] =
        "Servistinon",
        [PCX_TEXT_STRING_HANDMAID_DESCRIPTION] =
        "Ĝis via sekva vico, ignoru efikojn de kartoj de ĉiuj "
        "aliaj ludantoj.",
        [PCX_TEXT_STRING_PRINCE] =
        "Princo",
        [PCX_TEXT_STRING_PRINCE_OBJECT] =
        "Princon",
        [PCX_TEXT_STRING_PRINCE_DESCRIPTION] =
        "Elektu ludanton (povas esti vi) kiu forĵetos sian manon "
        "kaj prenos novan karton.",
        [PCX_TEXT_STRING_KING] =
        "Reĝo",
        [PCX_TEXT_STRING_KING_OBJECT] =
        "Reĝon",
        [PCX_TEXT_STRING_KING_DESCRIPTION] =
        "Elektu alian ludanton kaj interŝanĝu manojn kun ri.",
        [PCX_TEXT_STRING_COMTESSE] =
        "Grafino",
        [PCX_TEXT_STRING_COMTESSE_OBJECT] =
        "Grafinon",
        [PCX_TEXT_STRING_COMTESSE_DESCRIPTION] =
        "Se vi havas ĉi tiun karton kun la reĝo aŭ la princo en "
        "via mano, vi devas forĵeti ĉi tiun karton.",
        [PCX_TEXT_STRING_PRINCESS] =
        "Princino",
        [PCX_TEXT_STRING_PRINCESS_OBJECT] =
        "Princinon",
        [PCX_TEXT_STRING_PRINCESS_DESCRIPTION] =
        "Se vi forĵetas ĉi tiun karton, vi perdas la raŭndon.",
        [PCX_TEXT_STRING_ONE_COPY] =
        "1 ekzemplero",
        [PCX_TEXT_STRING_PLURAL_COPIES] =
        "%i ekzempleroj",
        [PCX_TEXT_STRING_YOUR_CARD_IS] =
        "Via karto estas: ",
        [PCX_TEXT_STRING_VISIBLE_CARDS] =
        "Forĵetitaj kartoj: ",
        [PCX_TEXT_STRING_N_CARDS] =
        "Kartaro: ",
        [PCX_TEXT_STRING_YOUR_GO_NO_QUESTION] =
        "<b>%p</b>, estas via vico",
        [PCX_TEXT_STRING_DISCARD_WHICH_CARD] =
        "Kiun karton vi volas forĵeti?",
        [PCX_TEXT_STRING_EVERYONE_PROTECTED] =
        "%p forĵetas la %C sed ĉiuj aliaj ludantoj "
        "estas protektataj kaj ĝi ne havas efikon.",
        [PCX_TEXT_STRING_WHO_GUESS] =
        "Kies karton vi volas diveni?",
        [PCX_TEXT_STRING_GUESS_WHICH_CARD] =
        "Kiun karton vi volas diveni?",
        [PCX_TEXT_STRING_GUARD_SUCCESS] =
        "%p forĵetis la %C kaj ĝuste divenis ke "
        "%p havis la %C. %p perdas la raŭndon.",
        [PCX_TEXT_STRING_GUARD_FAIL] =
        "%p forĵetis la %C kaj malĝuste divenis "
        "ke %p havas la %C.",
        [PCX_TEXT_STRING_WHO_SEE_CARD] =
        "Kies karton vi volas vidi?",
        [PCX_TEXT_STRING_SHOWS_CARD] =
        "%p forĵetis la %C kaj devigis %p "
        "sekrete montri sian karton.",
        [PCX_TEXT_STRING_TELL_SPIED_CARD] =
        "%p havas la %C",
        [PCX_TEXT_STRING_WHO_COMPARE] =
        "Kun kiu vi volas kompari kartojn?",
        [PCX_TEXT_STRING_COMPARE_LOSER] =
        "%p forĵetis la %C kaj komparis sian "
        "karton kun tiu de %p. La karto de %p estas "
        "malpli alta kaj ri perdas la raŭndon.",
        [PCX_TEXT_STRING_TELL_COMPARE] =
        "Vi havas la %C kaj %p havas la %C",
        [PCX_TEXT_STRING_COMPARE_CARDS_EQUAL] =
        "%p forĵetis la %C kaj komparis sian "
        "karton kun tiu de %p. La du kartoj estas "
        "egalaj kaj neniu perdas la raŭndon.",
        [PCX_TEXT_STRING_DISCARDS_HANDMAID] =
        "%p forĵetas la %C kaj estos protektata ĝis "
        "sia sekva vico",
        [PCX_TEXT_STRING_WHO_PRINCE] =
        "Kiun vi volas devigi forĵeti sian manon?",
        [PCX_TEXT_STRING_PRINCE_SELF] =
        "%p forĵetis la %C kaj devigis sin mem "
        "forĵeti sian %C",
        [PCX_TEXT_STRING_PRINCE_OTHER] =
        "%p forĵetis la %C kaj devigis %p "
        "forĵeti %C",
        [PCX_TEXT_STRING_FORCE_DISCARD_PRINCESS] =
        " kaj tial ri perdas la raŭndon.",
        [PCX_TEXT_STRING_FORCE_DISCARD_OTHER] =
        " kaj preni novan karton.",
        [PCX_TEXT_STRING_WHO_EXCHANGE] =
        "Kun kiu vi volas interŝanĝi manojn?",
        [PCX_TEXT_STRING_TELL_EXCHANGE] =
        "Vi fordonas la %C al %p kaj ricevas la %C",
        [PCX_TEXT_STRING_EXCHANGES] =
        "%p forĵetis la reĝon kaj interŝanĝas la manon kun %p",
        [PCX_TEXT_STRING_DISCARDS_COMTESSE] =
        "%p forĵetis la %C",
        [PCX_TEXT_STRING_DISCARDS_PRINCESS] =
        "%p forĵetas la %C kaj perdas la raŭndon",
        [PCX_TEXT_STRING_EVERYBODY_SHOWS_CARD] =
        "La raŭndo finiĝas kaj ĉiu montras sian karton:",
        [PCX_TEXT_STRING_SET_ASIDE_CARD] =
        "La kaŝita karto estis %c",
        [PCX_TEXT_STRING_WINS_ROUND] =
        "💘 %p gajnas la raŭndon kaj gajnas korinklinon "
        "de la princino",
        [PCX_TEXT_STRING_WINS_PRINCESS] =
        "🏆 %p havas %i korinklinojn kaj gajnas la partion!",
        [PCX_TEXT_STRING_FIGHTERS_ARE] =
        "La batalantoj en la sekva rondo estas:\n\n"
        "%s\n"
        "%s\n"
        "\n"
        "Ili nun elektas siajn batalantojn.",
        [PCX_TEXT_STRING_POSSIBLE_ROLES] =
        "Viaj rolkartoj estas:",
        [PCX_TEXT_STRING_POSSIBLE_ATTRIBUTES] =
        "Viaj trajtkartoj estas:",
        [PCX_TEXT_STRING_CHOOSE_ROLE] =
        "Bonvolu elekti rolon.",
        [PCX_TEXT_STRING_CHOOSE_ATTRIBUTE] =
        "Bonvolu elekti trajton.",
        [PCX_TEXT_STRING_YOUR_FIGHTER_IS] =
        "Dankon. Via batalanto estas:",
        [PCX_TEXT_STRING_FIGHTERS_CHOSEN] =
        "La batalantoj estas pretaj! Ili estas:",
        [PCX_TEXT_STRING_NOW_ARGUE] =
        "Vi ambaŭ devas nun argumenti kial via batalanto gajnus. Ek!",
        [PCX_TEXT_STRING_DONT_FORGET_TO_VOTE] =
        "Ne forgesu voĉdoni! La aktualaj voĉdonoj estas:",
        [PCX_TEXT_STRING_YOU_CAN_VOTE] =
        "Ĉu la debato jam finiĝis? La aliaj ludantoj povas nun voĉdoni "
        "per la subaj butonoj aŭ atendi plian disputadon.",
        [PCX_TEXT_STRING_X_VOTED_Y] =
        "%s voĉdonis por %s",
        [PCX_TEXT_STRING_CURRENT_VOTES_ARE] =
        "La aktualaj voĉdonoj estas:",
        [PCX_TEXT_STRING_FIGHT_EQUAL_RESULT] =
        "La rezulto estas egala! Nun komenciĝos decida batalo sen aldonaj "
        "trajtoj!",
        [PCX_TEXT_STRING_FIGHT_WINNER_IS] =
        "%s venkis la batalon! La aktualaj poentoj estas:",
        [PCX_TEXT_STRING_STAYS_ON] =
        "La unua homo kiu gajnos %i poentojn gajnas la partion. "
        "%s restos por la sekva batalo sen ŝanĝi sian batalanton.",
        [PCX_TEXT_STRING_THROW] =
        "Ĵeti la kubojn",
        [PCX_TEXT_STRING_STOP] =
        "Ĉesi",
        [PCX_TEXT_STRING_STOP_SCORE] =
        "%p ĉesas kaj aldonas %i al siaj poentoj.",
        [PCX_TEXT_STRING_THROW_FIRST_DICE] =
        "<b>%p</b>, estas via vico, premu la butonon por ĵeti la kubojn.",
        [PCX_TEXT_STRING_YOUR_DICE_ARE] =
        "Viaj ĵetkuboj estas:",
        [PCX_TEXT_STRING_THROWING_DICE] =
        "Ĵetas kubojn…",
        [PCX_TEXT_STRING_SCORE_SO_FAR] =
        "Poentoj ĝis nun:",
        [PCX_TEXT_STRING_DICE_IN_HAND] =
        "En via mano:",
        [PCX_TEXT_STRING_NO_DICE_IN_HAND] =
        "nenio",
        [PCX_TEXT_STRING_REMAINING_DICE_IN_BOX] =
        "Ĵetkuboj en la skatolo:",
        [PCX_TEXT_STRING_YOU_ARE_DEAD] =
        "La homoj pafis vin tro da fojoj kaj vi perdas ĉiujn viajn poentojn de "
        "ĉi tiu vico!",
        [PCX_TEXT_STRING_THROW_OR_STOP] =
        "Ĉu vi volas denove ĵeti la kubojn aŭ ĉesi nun?",
        [PCX_TEXT_STRING_START_LAST_ROUND] =
        "%p atingis %i poentojn do ĉi tiu estas la lasta raŭndo",
        [PCX_TEXT_STRING_WINS] =
        "🏆 <b>%p</b> gajnis la partion!",
        [PCX_TEXT_STRING_FINAL_SCORES] =
        "La finaj poentoj estas:",
        [PCX_TEXT_STRING_EVERYBODY_CHOOSE_CARD] =
        "Ĉiu nun devas elekti kiun karton ludi.",
        [PCX_TEXT_STRING_WHICH_CARD_TO_PLAY] =
        "Kiun karton vi volas ludi?",
        [PCX_TEXT_STRING_CARD_CHOSEN] =
        "Vi elektis:",
        [PCX_TEXT_STRING_CHOSEN_CARDS_ARE] =
        "Ĉiu elektis! La kartoj estas:",
        [PCX_TEXT_STRING_ADDED_TO_ROW] =
        "%s aldonas sian karton al linio %c.",
        [PCX_TEXT_STRING_ROW_FULL] =
        "La linio estas plena do ri devas preni ĝin kaj aldoni %i 🐮 "
        "al siaj poentoj.",
        [PCX_TEXT_STRING_CHOOSE_ROW] =
        "%s, via karto estas malpli alta ol ĉiu linio. Vi devas elekti linion "
        "kaj preni ĝin.",
        [PCX_TEXT_STRING_CHOSEN_ROW] =
        "%s prenas linion %c kaj aldonas %i 🐮 al siaj poentoj.",
        [PCX_TEXT_STRING_ROUND_OVER] =
        "La raŭndo finiĝis kaj la poentoj nun estas:",
        [PCX_TEXT_STRING_END_POINTS] =
        "%s havas almenaŭ %i poentojn kaj finas la partion.",
        [PCX_TEXT_STRING_WINS_PLAIN] =
        "🏆 %s gajnis la partion!",
        [PCX_TEXT_STRING_YOU_ARE_LEADER] =
        "%s komencas la prenvicon.",
        [PCX_TEXT_STRING_PLAYER_PLAYED] =
        "%s ludis:",
        [PCX_TEXT_STRING_FOLLOW_PLAYER] =
        "Nun %s elektas kiun karton ludi.",
        [PCX_TEXT_STRING_PLAYED_THREE] =
        "Nun ri elektas ĉu interŝanĝi la dekretan karton.",
        [PCX_TEXT_STRING_PLAYED_FIVE] =
        "Nun ri prenas karton de la kartaro kaj forĵetas unu.",
        [PCX_TEXT_STRING_TRICK_WINNER] =
        "%s gajnis la prenvicon.",
        [PCX_TEXT_STRING_TRICKS_IN_ROUND_ARE] =
        "La prenoj gajnitaj en ĉi tiu raŭndo ĝis nun estas:",
        [PCX_TEXT_STRING_WIN_TRICK_SEVEN] =
        "Ri tuj gajnas poenton pro la karto 7.",
        [PCX_TEXT_STRING_WIN_TRICK_TWO_SEVENS] =
        "Ri tuj gajnas du poentojn pro la du 7oj.",
        [PCX_TEXT_STRING_YOU_DREW] =
        "Vi prenas:",
        [PCX_TEXT_STRING_WHICH_CARD_DISCARD] =
        "Kiun karton vi volas forĵeti?",
        [PCX_TEXT_STRING_TRUMP_CARD_IS] =
        "La dekreta karto estas:",
        [PCX_TEXT_STRING_WHICH_CARD_EXCHANGE] =
        "Kiun karton vi volas meti kiel la dekretan karton?",
        [PCX_TEXT_STRING_DONT_EXCHANGE] =
        "Lasi la antaŭan dekretan karton",
        [PCX_TEXT_STRING_DOESNT_EXCHANGE] =
        "%s decidis ne interŝanĝi la dekretan karton.",
        [PCX_TEXT_STRING_TYPE_A_WORD] =
        "Tajpu vorton kiu enhavas:",
        [PCX_TEXT_STRING_GAME_OVER_WINNER] =
        "La ludo finiĝis. La venkinto estas…",
        [PCX_TEXT_STRING_LOST_ALL_LIVES] =
        "%s prenis tro da tempo kaj perdis sian lastan vivon.",
        [PCX_TEXT_STRING_LOST_A_LIFE] =
        "%s prenis tro da tempo kaj perdis vivon.",
        [PCX_TEXT_STRING_ALPHABET] =
        "abcĉdefgĝhĥijĵklmnoprsŝtuŭvz",
        [PCX_TEXT_STRING_ONE_UP] =
        "%s uzis la tutan alfabeton kaj gajnis bonusan vivon.",
        [PCX_TEXT_STRING_LETTERS_HINT] =
        "Uzu la jenajn literojn por regajni vivon:",
        [PCX_TEXT_STRING_WORDS_ARE] =
        "La vortolisto estas:",
        [PCX_TEXT_STRING_SECRET_WORD_IS] =
        "La sekreta vorto estas:",
        [PCX_TEXT_STRING_YOU_ARE_THE_CHAMELEON] =
        "Vi estas la kameleono 🦎",
        [PCX_TEXT_STRING_CLUE_QUESTION] =
        "%p, bonvolu tajpi vian indikon",
        [PCX_TEXT_STRING_START_DEBATE] =
        "Nun vi devas debati pri kiun vi kredas esti la kameleono. "
        "Kiam vi estos pretaj vi povos voĉdoni.",
        [PCX_TEXT_STRING_YOU_CAN_VOTE_FOR_A_PLAYER] =
        "Se vi jam finis la debaton, vi povas voĉdoni por la ludanto "
        "kiun vi suspektas esti la kameleono.",
        [PCX_TEXT_STRING_PLAYER_VOTED] =
        "%s voĉdonis",
        [PCX_TEXT_STRING_EVERYBODY_VOTED] =
        "Ĉiu voĉdonis!",
        [PCX_TEXT_STRING_ITS_A_DRAW] =
        "Estas egala rezulto! %p havas la decidan voĉdonon.",
        [PCX_TEXT_STRING_CHOSEN_PLAYER] =
        "La elektita ludanto estas %p.",
        [PCX_TEXT_STRING_YOU_FOUND_THE_CHAMELEON] =
        "Vi sukcese trovis la kameleonon! 🦎",
        [PCX_TEXT_STRING_YOU_DIDNT_FIND_THE_CHAMELEON] =
        "Tiu ne estas la kameleono!",
        [PCX_TEXT_STRING_CHAMELEON_WINS_POINTS] =
        "%p gajnas 2 poentojn kaj ĉiu alia gajnas nenion.",
        [PCX_TEXT_STRING_SCORES] =
        "Poentoj:",
        [PCX_TEXT_STRING_NOW_GUESS] =
        "%p, nun provu diveni la sekretan vorton.",
        [PCX_TEXT_STRING_CHAMELEON_GUESSED] =
        "La kameleono divenis %p.",
        [PCX_TEXT_STRING_CORRECT_GUESS] =
        "Tio estis la ĝusta vorto!",
        [PCX_TEXT_STRING_CORRECT_WORD_IS] =
        "La ĝusta sekreta vorto estas %p.",
        [PCX_TEXT_STRING_SECRET_WORD_WAS] =
        "La sekreta vorto estis %p.",
        [PCX_TEXT_STRING_ESCAPED_SCORE] =
        "%p gajnas unu poenton kaj ĉiu alia gajnas nenion.",
        [PCX_TEXT_STRING_CAUGHT_SCORE] =
        "Ĉiu krom %p gajnas 2 poentojn.",
        [PCX_TEXT_STRING_START_ROUND_BUTTON] =
        "Komenci sekvan raŭndon",
        [PCX_TEXT_STRING_WHICH_DECK_MODE] =
        "Per kiu reĝimo vi volas ludi?",
        [PCX_TEXT_STRING_DECK_MODE_BASIC] =
        "Baza",
        [PCX_TEXT_STRING_DECK_MODE_MOONSTRUCK] =
        "Plenluno",
        [PCX_TEXT_STRING_DECK_MODE_LONELY_NIGHT] =
        "Sola lupo",
        [PCX_TEXT_STRING_DECK_MODE_CONFUSION] =
        "Konfuzo",
        [PCX_TEXT_STRING_DECK_MODE_PAYBACK] =
        "Venĝo",
        [PCX_TEXT_STRING_DECK_MODE_SECRET_COMPANIONS] =
        "Sekretaj kunuloj",
        [PCX_TEXT_STRING_DECK_MODE_HOUSE_OF_DESPAIR] =
        "Necerteco",
        [PCX_TEXT_STRING_DECK_MODE_TWIGHLIGHT_ALLIANCE] =
        "Krepuska alianco",
        [PCX_TEXT_STRING_DECK_MODE_ANARCHY] =
        "Anarĥio",
        [PCX_TEXT_STRING_SHOW_ROLES] =
        "La vilaĝo konsistas el la sekvaj roluloj:",
        [PCX_TEXT_STRING_FALL_ASLEEP] =
        "Ĉiu rigardas sian rolon antaŭ ol ekdormi por la nokto.",
        [PCX_TEXT_STRING_TELL_ROLE] =
        "Via rolo estas:",
        [PCX_TEXT_STRING_VILLAGER] =
        "Vilaĝano",
        [PCX_TEXT_STRING_WEREWOLF] =
        "Homlupo",
        [PCX_TEXT_STRING_MINION] =
        "Sbiro",
        [PCX_TEXT_STRING_MASON] =
        "Masonisto",
        [PCX_TEXT_STRING_SEER] =
        "Klarvidulino",
        [PCX_TEXT_STRING_ROBBER] =
        "Ŝtelisto",
        [PCX_TEXT_STRING_TROUBLEMAKER] =
        "Petolulo",
        [PCX_TEXT_STRING_DRUNK] =
        "Ebriulo",
        [PCX_TEXT_STRING_INSOMNIAC] =
        "Sendormulo",
        [PCX_TEXT_STRING_HUNTER] =
        "Ĉasisto",
        [PCX_TEXT_STRING_TANNER] =
        "Tanisto",
        [PCX_TEXT_STRING_WEREWOLF_PHASE] =
        "🐺 La homlupoj vekiĝas kaj rigardas unu la alian antaŭ ol "
        "reendormiĝi.",
        [PCX_TEXT_STRING_LONE_WOLF] =
        "Vi estas la sola homlupo! Vi rajtas rigardi karton de la mezo de la "
        "tablo. Tiu karto estas:",
        [PCX_TEXT_STRING_WEREWOLVES_ARE] =
        "La homlupoj de la vilaĝoj estas:",
        [PCX_TEXT_STRING_MINION_PHASE] =
        "🦺 La sbiro vekiĝas kaj ekscias kiuj estas la homlupoj.",
        [PCX_TEXT_STRING_NO_WEREWOLVES] =
        "Neniu estas homlupo!",
        [PCX_TEXT_STRING_MASON_PHASE] =
        "⚒️ La masonistoj vekiĝas kaj rigardas unu la alian antaŭ ol "
        "reendormiĝi.",
        [PCX_TEXT_STRING_LONE_MASON] =
        "Vi estas la sola masonisto.",
        [PCX_TEXT_STRING_MASONS_ARE] =
        "La masonistoj de la vilaĝo estas:",
        [PCX_TEXT_STRING_SEER_PHASE] =
        "🔮 La klarvidulino vekiĝas kaj rajtas rigardi karton de aliulo aŭ "
        "du kartojn de la mezo de la tablo.",
        [PCX_TEXT_STRING_TWO_CARDS_FROM_THE_CENTER] =
        "Du kartojn de la tablomezo",
        [PCX_TEXT_STRING_SHOW_TWO_CARDS_FROM_CENTER] =
        "Du el la kartoj el la mezo de la tablo estas:",
        [PCX_TEXT_STRING_SHOW_PLAYER_CARD] =
        "La rolo de %s estas:",
        [PCX_TEXT_STRING_ROBBER_PHASE] =
        "🤏 La ŝtelisto vekiĝas kaj rajtas interŝanĝi sian karton kun tiu de "
        "alia ludanto. Se li faras tion li rajtas rigardi la karton.",
        [PCX_TEXT_STRING_WHO_TO_ROB] =
        "De kiu vi volas ŝteli?",
        [PCX_TEXT_STRING_NOBODY] =
        "Neniu",
        [PCX_TEXT_STRING_STEAL_FROM] =
        "Vi prenas la karton de antaŭ %s kaj donas al ri la vian. Ria karto "
        "estis:",
        [PCX_TEXT_STRING_ROBBED_NOBODY] =
        "Vi konservas la karton kiun vi jam havas kaj ŝtelas de neniu.",
        [PCX_TEXT_STRING_TROUBLEMAKER_PHASE] =
        "🐈 La petolulo vekiĝas kaj rajtas interŝanĝi la kartojn de du aliaj "
        "ludantoj.",
        [PCX_TEXT_STRING_FIRST_SWAP] =
        "Elektu la ludanton kies karton vi volas interŝanĝi.",
        [PCX_TEXT_STRING_SECOND_SWAP] =
        "Bone, nun elektu la duan ludanton kies karton vi volas interŝanĝi.",
        [PCX_TEXT_STRING_SWAPPED_NOBODY] =
        "Vi ne petolemas ĉi-nokte kaj ne interŝanĝas kartojn.",
        [PCX_TEXT_STRING_SWAP_CARDS_OF] =
        "Vi interŝanĝas la kartojn de %s kaj %s.",
        [PCX_TEXT_STRING_DRUNK_PHASE] =
        "🍺 La ebriulo vekiĝas konfuzite kaj interŝanĝas sian karton kun unu "
        "el la mezo de la tablo. Li ne plu scias kiu rolo li estas.",
        [PCX_TEXT_STRING_INSOMNIAC_PHASE] =
        "🥱 La sendormulo vekiĝas kaj kontrolas sian karton por vidi ĉu ŝi "
        "ankoaraŭ estas la sendormulo.",
        [PCX_TEXT_STRING_STILL_INSOMNIAC] =
        "Vi ankoraŭ estas la sendormulo.",
        [PCX_TEXT_STRING_YOU_ARE_NOW] =
        "Via karto nun estas:",
        [PCX_TEXT_STRING_EVERYONE_WAKES_UP] =
        "🌅 La suno leviĝas. Ĉiu loĝanto de la vilaĝo vekiĝas kaj komencas "
        "diskuti pri kiun ri kredas esti homlupo.",
        [PCX_TEXT_STRING_YOU_CAN_VOTE_FOR_A_WEREWOLF] =
        "Se vi jam finis diskuti, vi povas voĉdoni por tiu kiun vi kredas "
        "estas homlupo. Vi rajtas ŝanĝi vian voĉon ĝis ĉiu voĉdonos.",
        [PCX_TEXT_STRING_CHANGED_VOTE] =
        "%s ŝanĝis sian voĉon.",
        [PCX_TEXT_STRING_CANT_VOTE_SELF] =
        "Vi ne rajtas voĉdoni por vi mem.",
        [PCX_TEXT_STRING_VOTES_ARE] =
        "La voĉdonoj estis:",
        [PCX_TEXT_STRING_NO_ONE_DIES] =
        "Neniu ricevis pli ol unu voĉdonon do neniu mortas!",
        [PCX_TEXT_STRING_NO_WEREWOLVES_AT_END] =
        "Estis neniu homlupo je la fino de la ludo!",
        [PCX_TEXT_STRING_HOWEVER_ONE_WEREWOLF] =
        "Tamen, %s estas homlupo!",
        [PCX_TEXT_STRING_HOWEVER_MULTIPLE_WEREWOLVES] =
        "Tamen, %s estas homlupoj!",
        [PCX_TEXT_STRING_THEIR_ROLE] =
        "Ria rolo estis:",
        [PCX_TEXT_STRING_SACRIFICE] =
        "La vilaĝo elektis linĉi %s.",
        [PCX_TEXT_STRING_MULTIPLE_SACRIFICES] =
        "La vilaĝo elektis linĉi la jenajn homojn:",
        [PCX_TEXT_STRING_HUNTER_KILLS] =
        "Mortante, la ĉasisto pafas kaj mortigas %s.",
        [PCX_TEXT_STRING_VILLAGERS_WIN] =
        "🧑‍🌾 La vilaĝanoj venkis! 🧑‍🌾",
        [PCX_TEXT_STRING_WEREWOLVES_WIN] =
        "🐺 La homlupoj venkis! 🐺",
        [PCX_TEXT_STRING_MINION_WINS] =
        "🦺 La sbiro venkis! 🦺",
        [PCX_TEXT_STRING_NOBODY_WINS] =
        "🤦 Neniu venkis 🤦",
        [PCX_TEXT_STRING_VILLAGE_AND_TANNER_WIN] =
        "🙍‍♂️🧑‍🌾 La tanisto KAJ la vilaĝanoj venkis! 🧑‍🌾🙍‍♂️",
        [PCX_TEXT_STRING_TANNER_WINS] =
        "🙍‍♂️ La tanisto venkis! 🙍‍♂️",
};

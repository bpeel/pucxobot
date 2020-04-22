/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2019, 2020  Neil Roberts
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

#include "pcx-text-french.h"

#include "pcx-text.h"

const char *
pcx_text_french[] = {
        [PCX_TEXT_STRING_LANGUAGE_CODE] =
        "fr",
        [PCX_TEXT_STRING_NAME_COUP] =
        "Complot",
        [PCX_TEXT_STRING_NAME_SNITCH] =
        "Balance",
        [PCX_TEXT_STRING_NAME_LOVE] =
        "Love Letter",
        [PCX_TEXT_STRING_NAME_ZOMBIE] =
        "Zombie Dice",
        [PCX_TEXT_STRING_COUP_START_COMMAND] =
        "/complot",
        [PCX_TEXT_STRING_COUP_START_COMMAND_DESCRIPTION] =
        "Créer un jeu de Complot",
        [PCX_TEXT_STRING_SNITCH_START_COMMAND] =
        "/balance",
        [PCX_TEXT_STRING_SNITCH_START_COMMAND_DESCRIPTION] =
        "Créer un jeu de Balance",
        [PCX_TEXT_STRING_LOVE_START_COMMAND] =
        "/letter",
        [PCX_TEXT_STRING_LOVE_START_COMMAND_DESCRIPTION] =
        "Créer un jeu de Love Letter",
        [PCX_TEXT_STRING_ZOMBIE_START_COMMAND] =
        "/zombie",
        [PCX_TEXT_STRING_ZOMBIE_START_COMMAND_DESCRIPTION] =
        "Créer un jeu de Zombie Dice",
        [PCX_TEXT_STRING_WHICH_HELP] =
        "Pour quel jeu voulez-vous de l’aide ?",
        [PCX_TEXT_STRING_TIMEOUT_START] =
        "Personne n’a rejoint pendant plus que %i minutes. "
        "La partie commencera tout de suite.",
        [PCX_TEXT_STRING_TIMEOUT_ABANDON] =
        "La partie a été inactive pendant plus que %i minutes "
        "et sera abandonnée.",
        [PCX_TEXT_STRING_NEED_PUBLIC_GROUP] =
        "Pour jouer, ajoutez le bot à un groupe avec vos amis et commencez le "
        "jeu là-bas.",
        [PCX_TEXT_STRING_SEND_PRIVATE_MESSAGE] =
        "Avant de rejoindre un jeu, veuillez cliquer sur @%s et envoyer un "
        "message pour que j’aie le droit de vous envoyer vos cartes dans un "
        "message privé. Après avoir fait ça vous pouvez retourner ici pour "
        "rejoindre un jeu.",
        [PCX_TEXT_STRING_ALREADY_IN_GAME] =
        "Vous êtes déjà dans une partie",
        [PCX_TEXT_STRING_ALREADY_GAME] =
        "Il y a déjà une partie dans ce groupe",
        [PCX_TEXT_STRING_WHICH_GAME] =
        "À quel jeu voulez-vous jouer ?",
        [PCX_TEXT_STRING_GAME_FULL] =
        "La partie est complete",
        [PCX_TEXT_STRING_GAME_ALREADY_STARTED] =
        "La partie a déjà commencé",
        [PCX_TEXT_STRING_NO_GAME] =
        "Il n’y a actuellement aucune partie dans ce groupe.",
        [PCX_TEXT_STRING_CANCELED] =
        "Partie annulée.",
        [PCX_TEXT_STRING_CANT_CANCEL] =
        "Seulement les joueurs dans la partie peuvent l’annuler.",
        [PCX_TEXT_STRING_NAME_FROM_ID] =
        "M.%s",
        [PCX_TEXT_STRING_FINAL_CONJUNCTION] =
        " et ",
        [PCX_TEXT_STRING_FINAL_DISJUNCTION] =
        " ou ",
        [PCX_TEXT_STRING_WELCOME] =
        "Bienvenue. Les autres joueurs peuvent taper /rejoindre pour rejoindre "
        "la partie ou vous pouvez taper /commencer pour la commencer.\n"
        "\n"
        "Le jeu est : %s\n"
        "\n"
        "Les joueurs actuels sont :\n"
        "%s",
        [PCX_TEXT_STRING_JOIN_BEFORE_START] =
        "Rejoignez la partie en tapant /rejoindre avant de la commencer",
        [PCX_TEXT_STRING_NEED_MIN_PLAYERS] =
        "Il faut au moins %i joeurs pour jouer",
        [PCX_TEXT_STRING_JOIN_COMMAND] =
        "/rejoindre",
        [PCX_TEXT_STRING_JOIN_COMMAND_DESCRIPTION] =
        "Rejoindre un jeu déjà créé",
        [PCX_TEXT_STRING_START_COMMAND] =
        "/commencer",
        [PCX_TEXT_STRING_START_COMMAND_DESCRIPTION] =
        "Commencer un jeu",
        [PCX_TEXT_STRING_CANCEL_COMMAND] =
        "/annuler",
        [PCX_TEXT_STRING_CANCEL_COMMAND_DESCRIPTION] =
        "Annuler un jeu",
        [PCX_TEXT_STRING_HELP_COMMAND] =
        "/aide",
        [PCX_TEXT_STRING_HELP_COMMAND_DESCRIPTION] =
        "Montrer un résumé des règles",
        [PCX_TEXT_STRING_RECEIVED_PRIVATE_MESSAGE] =
        "Merci pour le message. Vous pouvez désormais rejoindre une partie "
        "dans un groupe public.",
        [PCX_TEXT_STRING_CHOOSE_GAME_TYPE] =
        "Veuillez choisir à quelle version du jeu vous voulez jouer",
        [PCX_TEXT_STRING_GAME_TYPE_CHOSEN] =
        "La version choisie est: %s",
        [PCX_TEXT_STRING_GAME_TYPE_ORIGINAL] =
        "Original",
        [PCX_TEXT_STRING_GAME_TYPE_INSPECTOR] =
        "Inquisiteur",
        [PCX_TEXT_STRING_GAME_TYPE_REFORMATION] =
        "Saint-Barthélémy",
        [PCX_TEXT_STRING_GAME_TYPE_REFORMATION_INSPECTOR] =
        "Saint-Barthélémy + Inquisiteur",
        [PCX_TEXT_STRING_COUP] =
        "Assassinat",
        [PCX_TEXT_STRING_INCOME] =
        "Revenu",
        [PCX_TEXT_STRING_FOREIGN_AID] =
        "Aide étrangère",
        [PCX_TEXT_STRING_TAX] =
        "Taxe (Duchesse)",
        [PCX_TEXT_STRING_CONVERT] =
        "Conversion",
        [PCX_TEXT_STRING_EMBEZZLE] =
        "Détournement de fonds",
        [PCX_TEXT_STRING_ASSASSINATE] =
        "Assassiner (Assassin)",
        [PCX_TEXT_STRING_EXCHANGE] =
        "Échange (Ambassadeur)",
        [PCX_TEXT_STRING_EXCHANGE_INSPECTOR] =
        "Échange (Inquisiteur)",
        [PCX_TEXT_STRING_INSPECT] =
        "Consulter carte (Inquisiteur)",
        [PCX_TEXT_STRING_STEAL] =
        "Voler (Capitaine)",
        [PCX_TEXT_STRING_ACCEPT] =
        "Accepter",
        [PCX_TEXT_STRING_CHALLENGE] =
        "Mettre en doute",
        [PCX_TEXT_STRING_BLOCK] =
        "Bloquer",
        [PCX_TEXT_STRING_1_COIN] =
        "1 or",
        [PCX_TEXT_STRING_PLURAL_COINS] =
        "%i or",
        [PCX_TEXT_STRING_YOUR_CARDS_ARE] =
        "Vos cartes sont :",
        [PCX_TEXT_STRING_NOONE] =
        "Personne",
        [PCX_TEXT_STRING_WON_1] =
        "%s a gagné!",
        [PCX_TEXT_STRING_WON_PLURAL] =
        "%s ont gagné!",
        [PCX_TEXT_STRING_YOUR_GO] =
        "%s, c’est à vous, que voulez-vous faire ?",
        [PCX_TEXT_STRING_WHICH_CARD_TO_LOSE] =
        "Quelle carte voulez-vous perdre ?",
        [PCX_TEXT_STRING_CHALLENGE_SUCCEEDED] =
        "%s a mis %s en doute et il/elle n’avait pas %s et perd "
        "une carte",
        [PCX_TEXT_STRING_INVERTED_CHALLENGE_SUCCEEDED] =
        "%s a mis en doute et %s concède donc %s perd une carte.",
        [PCX_TEXT_STRING_CHALLENGE_FAILED] =
        "%s a mis %s en doute mais il/elle avait vraiment %s. %s perd "
        "une carte et %s reçoit un remplacement.",
        [PCX_TEXT_STRING_INVERTED_CHALLENGE_FAILED] =
        "%s a mis en doute et %s a montré %s. Donc %s change ses cartes et "
        "%s perd une carte.",
        [PCX_TEXT_STRING_CHOOSING_REVEAL] =
        "%s a mis en doute et maintenant %s choisit une carte à révéler.",
        [PCX_TEXT_STRING_CHOOSING_REVEAL_INVERTED] =
        "%s a mis en doute et maintenant %s choisit s’il/elle veut concéder.",
        [PCX_TEXT_STRING_ANNOUNCE_CHALLENGE] =
        "%s ne croit pas que vous aviez %s.\n"
        "Quelle carte voulez-vous montrer ?",
        [PCX_TEXT_STRING_ANNOUNCE_INVERTED_CHALLENGE] =
        "%s croit que vous avez en effet %s.\n"
        "Voulez-vous concéder ?",
        [PCX_TEXT_STRING_CONCEDE] =
        "Concéder",
        [PCX_TEXT_STRING_SHOW_CARDS] =
        "Montrer les cartes",
        [PCX_TEXT_STRING_NO_CHALLENGE_SO_BLOCK] =
        "Personne n’a mis en doute. L’action est bloquée.",
        [PCX_TEXT_STRING_CLAIM_CARDS_TO_BLOCK] =
        "%s prétend avoir %s et bloque l’action.",
        [PCX_TEXT_STRING_DOES_SOMEBODY_CHALLENGE] =
        "Est-ce que quelqu’un veut le mettre en doute ?",
        [PCX_TEXT_STRING_OR_BLOCK_NO_TARGET] =
        "Ou est-ce que quelqu’un veut prétendre avoir %s et le bloquer ?",
        [PCX_TEXT_STRING_BLOCK_NO_TARGET] =
        "Est-ce que quelqu’un veut prétendre avoir %s et le bloquer ?",
        [PCX_TEXT_STRING_OR_BLOCK_OTHER_ALLEGIANCE] =
        "Ou est-ce que quelqu’un d’une autre allégeance veut prétendre avoir "
        "%s et le bloquer ?",
        [PCX_TEXT_STRING_BLOCK_OTHER_ALLEGIANCE] =
        "Est-ce que quelqu’un d’une autre allégeance veut prétendre avoir "
        "%s et le bloquer ?",
        [PCX_TEXT_STRING_OR_BLOCK_WITH_TARGET] =
        "Ou %s, voulez-vous prétendre avoir %s et le bloquer ?",
        [PCX_TEXT_STRING_BLOCK_WITH_TARGET] =
        "%s, voulez-vous prétendre avoir %s et le bloquer ?",
        [PCX_TEXT_STRING_WHO_TO_COUP] =
        "%s, qui voulez-vous tuer pendant l’assassinat ?",
        [PCX_TEXT_STRING_DOING_COUP] =
        "💣 %s lance un assassinat contre %s",
        [PCX_TEXT_STRING_DOING_INCOME] =
        "💲 %s prend 1 or de revenu",
        [PCX_TEXT_STRING_REALLY_DOING_FOREIGN_AID] =
        "Personne n’a bloqué, %s prend les 2 or",
        [PCX_TEXT_STRING_DOING_FOREIGN_AID] =
        "💴 %s prend 2 or par aide étrangère.",
        [PCX_TEXT_STRING_EMBEZZLING] =
        "💼 %s prétend ne pas avoir la duchesse et il/elle détourne les fonds "
        "de l’hospice.",
        [PCX_TEXT_STRING_REALLY_EMBEZZLING] =
        "Personne n’a mis en doute, %s prend les fonds de l’hospice.",
        [PCX_TEXT_STRING_REALLY_DOING_TAX] =
        "Personne n’a mis en doute, %s prend les 2 or.",
        [PCX_TEXT_STRING_DOING_TAX] =
        "💸 %s prétend avoir la duchesse et prend 3 or au trésor.",
        [PCX_TEXT_STRING_WHO_TO_CONVERT] =
        "%s, qui voulez-vous convertir ?",
        [PCX_TEXT_STRING_CONVERTS_SELF] =
        "%s paie une pièce à l’hospice and se convertit à soi-même.",
        [PCX_TEXT_STRING_CONVERTS_SOMEONE_ELSE] =
        "%s paie 2 pièces à l’hospice et convertit %s.",
        [PCX_TEXT_STRING_REALLY_DOING_ASSASSINATION] =
        "Personne n’a bloqué ou mis en doute, %s assassine %s",
        [PCX_TEXT_STRING_SELECT_TARGET_ASSASSINATION] =
        "%s, qui voulez-vous assassiner ?",
        [PCX_TEXT_STRING_DOING_ASSASSINATION] =
        "🗡 %s veut assassiner %s",
        [PCX_TEXT_STRING_WHICH_CARDS_TO_KEEP] =
        "Quelles cartes voulez-vous garder ?",
        [PCX_TEXT_STRING_REALLY_DOING_EXCHANGE] =
        "Personne n’a mis en doute, %s échange des cartes.",
        [PCX_TEXT_STRING_DOING_EXCHANGE] =
        "🔄 %s prétend avoir l’ambassadeur et veut échanger des cartes.",
        [PCX_TEXT_STRING_DOING_EXCHANGE_INSPECTOR] =
        "🔄 %s prétend avoir l’inquisiteur et veut échanger des cartes.",
        [PCX_TEXT_STRING_REALLY_DOING_INSPECT] =
        "Personne n’a mis en doute, %s choisit une carte à montrer à %s",
        [PCX_TEXT_STRING_SELECT_TARGET_INSPECT] =
        "%s, de qui voulez-vous consulter une carte ?",
        [PCX_TEXT_STRING_DOING_INSPECT] =
        "🔍 %s prétend avoir l’inquisiteur et veut consulter une carte de %s",
        [PCX_TEXT_STRING_CHOOSE_CARD_TO_SHOW] =
        "Quelle carte voulez-vous montrer à %s?",
        [PCX_TEXT_STRING_OTHER_PLAYER_DECIDING_CAN_KEEP] =
        "%s est en train de décider si vous pouvez garder %s",
        [PCX_TEXT_STRING_SHOWING_CARD] =
        "%s vous montre %s. Est-ce qu’il peut le garder ?",
        [PCX_TEXT_STRING_YES] =
        "Oui",
        [PCX_TEXT_STRING_NO] =
        "Non",
        [PCX_TEXT_STRING_ALLOW_KEEP] =
        "%s a permis à %s de garder la carte qu’il a montrée.",
        [PCX_TEXT_STRING_DONT_ALLOW_KEEP] =
        "%s a obligé %s de changer la carte qu’il a montrée.",
        [PCX_TEXT_STRING_REALLY_DOING_STEAL] =
        "Personne n’a bloqué ou mis en doute, %s vole à %s",
        [PCX_TEXT_STRING_SELECT_TARGET_STEAL] =
        "%s, à qui voulez-vous voler ?",
        [PCX_TEXT_STRING_DOING_STEAL] =
        "💰 %s veut voler à %s.",
        [PCX_TEXT_STRING_CHARACTER_NAME_DUKE] =
        "Duchesse",
        [PCX_TEXT_STRING_CHARACTER_NAME_ASSASSIN] =
        "Assassin",
        [PCX_TEXT_STRING_CHARACTER_NAME_CONTESSA] =
        "Comtesse",
        [PCX_TEXT_STRING_CHARACTER_NAME_CAPTAIN] =
        "Capitaine",
        [PCX_TEXT_STRING_CHARACTER_NAME_AMBASSADOR] =
        "Ambassadeur",
        [PCX_TEXT_STRING_CHARACTER_NAME_INSPECTOR] =
        "Inquisiteur",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_DUKE] =
        "la duchesse",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_ASSASSIN] =
        "l’assassin",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CONTESSA] =
        "la comtesse",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CAPTAIN] =
        "le capitaine",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_AMBASSADOR] =
        "l’ambassadeur",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_INSPECTOR] =
        "l’inquisiteur",
        [PCX_TEXT_STRING_REUNIFICATION_OCCURED] =
        "La court est réunie. Tout le monde peut cibler n’importe qui et "
        "il n’y a plus de conversion.",
        [PCX_TEXT_STRING_COINS_IN_TREASURY] =
        "Hospice: %i",
        [PCX_TEXT_STRING_ROLE_NAME_DRIVER] =
        "Chauffeur",
        [PCX_TEXT_STRING_ROLE_NAME_LOCKPICK] =
        "Serrurier",
        [PCX_TEXT_STRING_ROLE_NAME_MUSCLE] =
        "Costaud",
        [PCX_TEXT_STRING_ROLE_NAME_CON_ARTIST] =
        "Arnaquer",
        [PCX_TEXT_STRING_ROLE_NAME_LOOKOUT] =
        "Guetteur",
        [PCX_TEXT_STRING_ROLE_NAME_SNITCH] =
        "Balance",
        [PCX_TEXT_STRING_ROUND_NUM] =
        "Tour %i / %i",
        [PCX_TEXT_STRING_CHOOSE_HEIST_DIFFICULTY] =
        "%s, vous êtes le chef. Combien de personnages voulez-vous pour "
        "le braquage ?",
        [PCX_TEXT_STRING_HEIST_SIZE_CHOSEN] =
        "Le braquage aura besoin de ces %i personnages :",
        [PCX_TEXT_STRING_DISCUSS_HEIST] =
        "Maintenant vous pouvez discuter entre vous sur quels personnages vous "
        "aller fournir au braquage. Quand vous serez prêts, choisissez votre "
        "carte en secret.",
        [PCX_TEXT_STRING_CARDS_CHOSEN] =
        "Tout le monde a fait son choix ! Les personnages sont :",
        [PCX_TEXT_STRING_NEEDED_CARDS_WERE] =
        "La cartes requises ont été :",
        [PCX_TEXT_STRING_YOU_CHOSE] =
        "Vous avez choisi :",
        [PCX_TEXT_STRING_WHICH_ROLE] =
        "Quelle carte voulez-vous choisir ?",
        [PCX_TEXT_STRING_HEIST_SUCCESS] =
        "Le braquage a réussi ! Tous les joueurs qui n’ont pas choisi "
        "la balance reçoivent %i or.",
        [PCX_TEXT_STRING_HEIST_FAILED] =
        "Le braquage a échoué ! Tous ce qui n’ont pas choisi la balance "
        "perdent 1 or.",
        [PCX_TEXT_STRING_SNITCH_GAIN_1] =
        "Tous les autres gagnent 1 or.",
        [PCX_TEXT_STRING_SNITCH_GAIN_PLURAL] =
        "Tous les autres gagnent %i or.",
        [PCX_TEXT_STRING_EVERYONE_SNITCHED] =
        "Le braquage a échoué et tout le monde l’a balancé ! "
        "Personne ne gagne d’or.",
        [PCX_TEXT_STRING_NOONE_SNITCHED] =
        "Personne ne l’a balancé.",
        [PCX_TEXT_STRING_1_SNITCH] =
        "1 balance",
        [PCX_TEXT_STRING_PLURAL_SNITCHES] =
        "%i balances",
        [PCX_TEXT_STRING_GUARD] =
        "Garde",
        [PCX_TEXT_STRING_GUARD_OBJECT] =
        "le garde",
        [PCX_TEXT_STRING_GUARD_DESCRIPTION] =
        "Évoquez une carte qui n’est pas le garde et choisissez un joueur. Si "
        "ce joueur a cette carte, il perd la manche.",
        [PCX_TEXT_STRING_SPY] =
        "Espion",
        [PCX_TEXT_STRING_SPY_OBJECT] =
        "l’espion",
        [PCX_TEXT_STRING_SPY_DESCRIPTION] =
        "Regardez la main d’un autre joueur.",
        [PCX_TEXT_STRING_BARON] =
        "Baron",
        [PCX_TEXT_STRING_BARON_OBJECT] =
        "le baron",
        [PCX_TEXT_STRING_BARON_DESCRIPTION] =
        "Comparez votre main avec celle d’un autre joueur en secret. La "
        "personne qui a la carte avec le moins de valeur perd la manche.",
        [PCX_TEXT_STRING_HANDMAID] =
        "Servante",
        [PCX_TEXT_STRING_HANDMAID_OBJECT] =
        "la servante",
        [PCX_TEXT_STRING_HANDMAID_DESCRIPTION] =
        "Jusqu’à votre prochaine tour, ignorez les effets des cartes des "
        "autres joueuers.",
        [PCX_TEXT_STRING_PRINCE] =
        "Prince",
        [PCX_TEXT_STRING_PRINCE_OBJECT] =
        "le prince",
        [PCX_TEXT_STRING_PRINCE_DESCRIPTION] =
        "Choisissez un joueur (qui peut être vous même) qui défaussera sa main "
        "et prendra une nouvelle carte.",
        [PCX_TEXT_STRING_KING] =
        "Roi",
        [PCX_TEXT_STRING_KING_OBJECT] =
        "le roi",
        [PCX_TEXT_STRING_KING_DESCRIPTION] =
        "Choisissez un autre joueur et échangez vos cartes avec lui.",
        [PCX_TEXT_STRING_COMTESSE] =
        "Comtesse",
        [PCX_TEXT_STRING_COMTESSE_OBJECT] =
        "la comtesse",
        [PCX_TEXT_STRING_COMTESSE_DESCRIPTION] =
        "Si vous avez cette carte avec le roi ou le prince dans votre main, "
        "il faut défausser cette carte.",
        [PCX_TEXT_STRING_PRINCESS] =
        "Princesse",
        [PCX_TEXT_STRING_PRINCESS_OBJECT] =
        "la princesse",
        [PCX_TEXT_STRING_PRINCESS_DESCRIPTION] =
        "Si vous défaussez cette carte, vous perdez la manche.",
        [PCX_TEXT_STRING_ONE_COPY] =
        "1 exemplaire",
        [PCX_TEXT_STRING_PLURAL_COPIES] =
        "%i exemplaires",
        [PCX_TEXT_STRING_YOUR_CARD_IS] =
        "Votre carte est : ",
        [PCX_TEXT_STRING_VISIBLE_CARDS] =
        "Cartes défaussées : ",
        [PCX_TEXT_STRING_N_CARDS] =
        "Pioche : ",
        [PCX_TEXT_STRING_YOUR_GO_NO_QUESTION] =
        "<b>%p</b>, c’est a vous",
        [PCX_TEXT_STRING_DISCARD_WHICH_CARD] =
        "Quelle carte voulez-vous défausser ?",
        [PCX_TEXT_STRING_EVERYONE_PROTECTED] =
        "%p défausse %C mais tous les autres joueurs sont protegés et il n’a "
        "pas d’effet.",
        [PCX_TEXT_STRING_WHO_GUESS] =
        "À qui voulez-vous diviner la carte ?",
        [PCX_TEXT_STRING_GUESS_WHICH_CARD] =
        "Quelle carte voulez-vous diviner ?",
        [PCX_TEXT_STRING_GUARD_SUCCESS] =
        "%p défausse %C et divine bien que %p avait %C. %p perd la manche.",
        [PCX_TEXT_STRING_GUARD_FAIL] =
        "%p défausse %C et divine à tort que %p a %C.",
        [PCX_TEXT_STRING_WHO_SEE_CARD] =
        "À qui voulez-vous voir sa carte ?",
        [PCX_TEXT_STRING_SHOWS_CARD] =
        "%p défaussé %C et force %p à montrer sa carte en secret.",
        [PCX_TEXT_STRING_TELL_SPIED_CARD] =
        "%p a %C",
        [PCX_TEXT_STRING_WHO_COMPARE] =
        "Avec qui voulez-vous comparer vos cartes ?",
        [PCX_TEXT_STRING_COMPARE_LOSER] =
        "%p a défaussé %C et a comparé sa carte avec celle de %p. "
        "La carte de %p a moins de valeur et il perd la manche.",
        [PCX_TEXT_STRING_TELL_COMPARE] =
        "Vous avez %C et %p a %C",
        [PCX_TEXT_STRING_COMPARE_CARDS_EQUAL] =
        "%p a défaussé %C et a comparé sa carte avec celle de %p. "
        "Les deux cartes étaient égaux et personne n’a perdu la manche.",
        [PCX_TEXT_STRING_DISCARDS_HANDMAID] =
        "%p défausse %C et sera protegé jusqu’à sa prochaine tour.",
        [PCX_TEXT_STRING_WHO_PRINCE] =
        "À qui voulez-vous faire défausser sa main ?",
        [PCX_TEXT_STRING_PRINCE_SELF] =
        "%p a défaussé %C et a forcé lui-même à défausser %C",
        [PCX_TEXT_STRING_PRINCE_OTHER] =
        "%p a défaussé %C et a forcé %p à défausser %C",
        [PCX_TEXT_STRING_FORCE_DISCARD_PRINCESS] =
        " et ainsi il perd la manche.",
        [PCX_TEXT_STRING_FORCE_DISCARD_OTHER] =
        " et prendre une nouvelle carte.",
        [PCX_TEXT_STRING_WHO_EXCHANGE] =
        "Avec qui voulez-vous échanger vos mains ?",
        [PCX_TEXT_STRING_TELL_EXCHANGE] =
        "Vous donnez %C à %p et recevez %C",
        [PCX_TEXT_STRING_EXCHANGES] =
        "%p défausse %C et échange sa main avec %p",
        [PCX_TEXT_STRING_DISCARDS_COMTESSE] =
        "%p défausse %C",
        [PCX_TEXT_STRING_DISCARDS_PRINCESS] =
        "%p défausse %C et perd la manche",
        [PCX_TEXT_STRING_EVERYBODY_SHOWS_CARD] =
        "La manche se termine et tout le monde montre sa carte :",
        [PCX_TEXT_STRING_SET_ASIDE_CARD] =
        "La carte cachée a été %c",
        [PCX_TEXT_STRING_WINS_ROUND] =
        "💘 %p remporte la manche et gagne de l’affection de la princesse",
        [PCX_TEXT_STRING_WINS_PRINCESS] =
        "🏆 %p a %i points d’affection et remporte la partie !",
        [PCX_TEXT_STRING_THROW] =
        "Lancer les dés",
        [PCX_TEXT_STRING_STOP] =
        "Arrêter",
        [PCX_TEXT_STRING_STOP_SCORE] =
        "%p arrête et ajoute %i à son score.",
        [PCX_TEXT_STRING_THROW_FIRST_DICE] =
        "<b>%p</b>, c’est à vous, appuyez sur le bouton pour lancer les dés.",
        [PCX_TEXT_STRING_YOUR_DICE_ARE] =
        "Vos dés sont:",
        [PCX_TEXT_STRING_THROWING_DICE] =
        "Vous lancez les dés…",
        [PCX_TEXT_STRING_SCORE_SO_FAR] =
        "Scores jusqu’ici:",
        [PCX_TEXT_STRING_REMAINING_DICE_IN_BOX] =
        "Dés dans la boîte:",
        [PCX_TEXT_STRING_YOU_ARE_DEAD] =
        "Vous vous êtes fait tirer dessus trop de fois et vous perdez tous "
        "vos points de ce tour !",
        [PCX_TEXT_STRING_THROW_OR_STOP] =
        "Voulez-vous encore lancer les dés ou arretez maintenent ?",
        [PCX_TEXT_STRING_START_LAST_ROUND] =
        "%p a atteint %i points donc ce sera la dernière manche.",
        [PCX_TEXT_STRING_WINS] =
        "🏆 <b>%p</b> remporte la partie !",
        [PCX_TEXT_STRING_FINAL_SCORES] =
        "Les scores finals sont :",
};

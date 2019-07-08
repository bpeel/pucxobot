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

#include "pcx-text-french.h"

#include "pcx-text.h"

const char *
pcx_text_french[] = {
        [PCX_TEXT_STRING_LANGUAGE_CODE] = "fr",
        [PCX_TEXT_STRING_NAME_COUP] =
        "Complot",
        [PCX_TEXT_STRING_NAME_SNITCH] =
        "Balance",
        [PCX_TEXT_STRING_WHICH_HELP] =
        "Pour quel jeu voulez-vous de l’aide ?",
        [PCX_TEXT_STRING_TIMEOUT_START] =
        "Personne n’a rejoint pendant plus que %i minutes. "
        "La partie commencera tout de suite.",
        [PCX_TEXT_STRING_TIMEOUT_ABANDON] =
        "La partie a été inactive pendant plus que %i minutes "
        "et sera abandonnée.",
        [PCX_TEXT_STRING_NEED_PUBLIC_GROUP] =
        "Veuillez rejoindre une partie dans un groupe publique.",
        [PCX_TEXT_STRING_SEND_PRIVATE_MESSAGE] =
        "Veuillez me envoyer un message privé à @%s "
        "pour que je puisse vous envoyer vos cartes discretement.",
        [PCX_TEXT_STRING_ALREADY_IN_GAME] =
        "Vous êtes déjà dans une partie",
        [PCX_TEXT_STRING_GAME_FULL] =
        "La partie est pleine",
        [PCX_TEXT_STRING_GAME_ALREADY_STARTED] =
        "La partie a déjà commencé",
        [PCX_TEXT_STRING_NAME_FROM_ID] =
        "M.%s",
        [PCX_TEXT_STRING_FINAL_CONJUNCTION] =
        " et ",
        [PCX_TEXT_STRING_FINAL_DISJUNCTION] =
        " ou ",
        [PCX_TEXT_STRING_WELCOME] =
        "Bienvenue. Les autres joueurs peuvent taper "
        "/rejoindre pour rejoindre la partie ou vous pouvez taper "
        "/commencer pour la commencer. Les joueurs actuels sont :\n"
        "%s",
        [PCX_TEXT_STRING_JOIN_BEFORE_START] =
        "Rejoignez la partie en tapant /rejoindre avant de la commencer",
        [PCX_TEXT_STRING_NEED_MIN_PLAYERS] =
        "Il faut au moins %i joeurs pour jouer",
        [PCX_TEXT_STRING_JOIN_COMMAND] =
        "/rejoindre",
        [PCX_TEXT_STRING_START_COMMAND] =
        "/commencer",
        [PCX_TEXT_STRING_HELP_COMMAND] =
        "/aide",
        [PCX_TEXT_STRING_RECEIVED_PRIVATE_MESSAGE] =
        "Merci pour le message. Vous pouvez désormais rejoindre une partie "
        "dans un groupe public",
        [PCX_TEXT_STRING_COUP] =
        "Assassinat",
        [PCX_TEXT_STRING_INCOME] =
        "Revenu",
        [PCX_TEXT_STRING_FOREIGN_AID] =
        "Aide étrangère",
        [PCX_TEXT_STRING_TAX] =
        "Taxe (Duchesse)",
        [PCX_TEXT_STRING_ASSASSINATE] =
        "Assassiner (Assassin)",
        [PCX_TEXT_STRING_EXCHANGE] =
        "Échange (Ambassadeur)",
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
        [PCX_TEXT_STRING_WON] =
        "%s a gagné!",
        [PCX_TEXT_STRING_YOUR_GO] =
        "%s, c’est à vous, que voulez-vous faire ?",
        [PCX_TEXT_STRING_WHICH_CARD_TO_LOSE] =
        "Quelle carte voulez-vous perdre ?",
        [PCX_TEXT_STRING_CHALLENGE_SUCCEEDED] =
        "%s a mis %s en doute et il/elle n’avait pas %s et perd "
        "une carte",
        [PCX_TEXT_STRING_CHALLENGE_FAILED] =
        "%s a mis %s en doute mais il/elle avait vraiment %s et %s perd "
        "une carte",
        [PCX_TEXT_STRING_ANNOUNCE_CHALLENGE] =
        "%s ne croit pas que vous aviez %s.\n"
        "Quelle carte voulez-vous montrer ?",
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
        [PCX_TEXT_STRING_REALLY_DOING_TAX] =
        "Personne n’a mis en doute, %s prend les 2 or.",
        [PCX_TEXT_STRING_DOING_TAX] =
        "💸 %s prétend avoir la duchesse et prend 3 or au trésor.",
        [PCX_TEXT_STRING_REALLY_DOING_ASSASSINATION] =
        "Personne n’a bloqué ou mis en doute, %s assassine %s",
        [PCX_TEXT_STRING_SELECT_TARGET_ASSASSINATION] =
        "%s, qui voulez-vous assassiner ?",
        [PCX_TEXT_STRING_DOING_ASSASSINATION] =
        "🗡 %s veut assassiner %s",
        [PCX_TEXT_STRING_WHICH_CARDS_TO_KEEP] =
        "Quelles cartes voulez-vous garder ?",
        [PCX_TEXT_STRING_REALLY_DOING_EXCHANGE] =
        "Personne n’a bloqué ou mis en doute, %s échange des cartes.",
        [PCX_TEXT_STRING_DOING_EXCHANGE] =
        "🔄 %s prétend avoir l’ambassadeur et veut échanger des cartes.",
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
        "%i balances"
};
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

#include "pcx-text-english.h"

#include "pcx-text.h"

const char *
pcx_text_english[] = {
        [PCX_TEXT_STRING_LANGUAGE_CODE] =
        "en",
        [PCX_TEXT_STRING_NAME_COUP] =
        "Coup",
        [PCX_TEXT_STRING_NAME_SNITCH] =
        "Snitch",
        [PCX_TEXT_STRING_NAME_LOVE] =
        "Love Letter",
        [PCX_TEXT_STRING_NAME_SIX] =
        "6 Takes!",
        [PCX_TEXT_STRING_NAME_FOX] =
        "Fox in the Forest",
        [PCX_TEXT_STRING_NAME_CHAMELEON] =
        "Chameleon",
        [PCX_TEXT_STRING_NAME_SUPERFIGHT] =
        "Superfight",
        [PCX_TEXT_STRING_NAME_ZOMBIE] =
        "Zombie Dice",
        [PCX_TEXT_STRING_COUP_START_COMMAND] =
        "/coup",
        [PCX_TEXT_STRING_COUP_START_COMMAND_DESCRIPTION] =
        "Create a game of Coup",
        [PCX_TEXT_STRING_SNITCH_START_COMMAND] =
        "/snitch",
        [PCX_TEXT_STRING_SNITCH_START_COMMAND_DESCRIPTION] =
        "Create a game of Snitch",
        [PCX_TEXT_STRING_LOVE_START_COMMAND] =
        "/letter",
        [PCX_TEXT_STRING_LOVE_START_COMMAND_DESCRIPTION] =
        "Create a game of Love Letter",
        [PCX_TEXT_STRING_SIX_START_COMMAND] =
        "/six",
        [PCX_TEXT_STRING_SIX_START_COMMAND_DESCRIPTION] =
        "Create a game of 6 Takes!",
        [PCX_TEXT_STRING_FOX_START_COMMAND] =
        "/fox",
        [PCX_TEXT_STRING_FOX_START_COMMAND_DESCRIPTION] =
        "Create a game of Fox in the Forest",
        [PCX_TEXT_STRING_CHAMELEON_START_COMMAND] =
        "/chameleon",
        [PCX_TEXT_STRING_CHAMELEON_START_COMMAND_DESCRIPTION] =
        "Create a game of Chameleon",
        [PCX_TEXT_STRING_SUPERFIGHT_START_COMMAND] =
        "/superfight",
        [PCX_TEXT_STRING_SUPERFIGHT_START_COMMAND_DESCRIPTION] =
        "Create a game of Superfight",
        [PCX_TEXT_STRING_ZOMBIE_START_COMMAND] =
        "/zombie",
        [PCX_TEXT_STRING_ZOMBIE_START_COMMAND_DESCRIPTION] =
        "Create a game of Zombie Dice",
        [PCX_TEXT_STRING_WHICH_HELP] =
        "Which game do you want help for?",
        [PCX_TEXT_STRING_TIMEOUT_START] =
        "Nobody joined for at least %i minutes. The game will start "
        "immediately.",
        [PCX_TEXT_STRING_TIMEOUT_ABANDON] =
        "The game has been inactive for at least %i minutes and will be "
        "abandoned.",
        [PCX_TEXT_STRING_NEED_PUBLIC_GROUP] =
        "To play a game, add the bot to a group chat with your friends and "
        "start the game there instead.",
        [PCX_TEXT_STRING_SEND_PRIVATE_MESSAGE] =
        "Before joining a game, you first need to click on @%s and send a "
        "message so that I will have permission to send you your cards in a "
        "private message. After doing that you can come back here to join "
        "the game.",
        [PCX_TEXT_STRING_ALREADY_IN_GAME] =
        "You’re already in a game.",
        [PCX_TEXT_STRING_ALREADY_GAME] =
        "There’s already a game in this group.",
        [PCX_TEXT_STRING_WHICH_GAME] =
        "Which game do you want to play?",
        [PCX_TEXT_STRING_GAME_FULL] =
        "The game is already full.",
        [PCX_TEXT_STRING_GAME_ALREADY_STARTED] =
        "The game has already started.",
        [PCX_TEXT_STRING_NO_GAME] =
        "There’s currently no game in this group.",
        [PCX_TEXT_STRING_CANCELED] =
        "Game canceled.",
        [PCX_TEXT_STRING_CANT_CANCEL] =
        "Only players in the game can cancel it.",
        [PCX_TEXT_STRING_NAME_FROM_ID] =
        "Mx.%i",
        [PCX_TEXT_STRING_FINAL_CONJUNCTION] =
        " and ",
        [PCX_TEXT_STRING_FINAL_DISJUNCTION] =
        " or ",
        [PCX_TEXT_STRING_WELCOME] =
        "Welcome. Other players can type /join to join the game or you can "
        "type /start to start it.",
        [PCX_TEXT_STRING_WELCOME_FULL] =
        "Welcome. The game is now full and will start immediately.",
        [PCX_TEXT_STRING_WELCOME_BUTTONS] =
        "%s joined the game. You can wait for more players or click the "
        "button below to begin.",
        [PCX_TEXT_STRING_WELCOME_BUTTONS_TOO_FEW] =
        "%s joined the game. Wait for some more players before starting the "
        "game.",
        [PCX_TEXT_STRING_WELCOME_BUTTONS_FULL] =
        "%s joined the game. The game is now full and will start immediately.",
        [PCX_TEXT_STRING_PLAYER_LEFT] =
        "%s left",
        [PCX_TEXT_STRING_START_BUTTON] =
        "Start",
        [PCX_TEXT_STRING_CHOSEN_GAME] =
        "Game: %s",
        [PCX_TEXT_STRING_CURRENT_PLAYERS] =
        "The current players are:",
        [PCX_TEXT_STRING_JOIN_BEFORE_START] =
        "Please join the game with /join before trying to start it.",
        [PCX_TEXT_STRING_NEED_MIN_PLAYERS] =
        "At least %i players are needed to play.",
        [PCX_TEXT_STRING_JOIN_COMMAND] =
        "/join",
        [PCX_TEXT_STRING_JOIN_COMMAND_DESCRIPTION] =
        "Join an existing game or create a new one",
        [PCX_TEXT_STRING_START_COMMAND] =
        "/start",
        [PCX_TEXT_STRING_START_COMMAND_DESCRIPTION] =
        "Start an existing game",
        [PCX_TEXT_STRING_CANCEL_COMMAND] =
        "/cancel",
        [PCX_TEXT_STRING_CANCEL_COMMAND_DESCRIPTION] =
        "Cancel an existing game",
        [PCX_TEXT_STRING_HELP_COMMAND] =
        "/help",
        [PCX_TEXT_STRING_HELP_COMMAND_DESCRIPTION] =
        "Show a summary of the rules",
        [PCX_TEXT_STRING_RECEIVED_PRIVATE_MESSAGE] =
        "Thanks for the message. You can now join a game in a public group.",
        [PCX_TEXT_STRING_CHOOSE_GAME_TYPE] =
        "Please choose which version of the game you want to play.",
        [PCX_TEXT_STRING_GAME_TYPE_CHOSEN] =
        "The chosen version is: %s",
        [PCX_TEXT_STRING_GAME_TYPE_ORIGINAL] =
        "Original",
        [PCX_TEXT_STRING_GAME_TYPE_INSPECTOR] =
        "Inquisitor",
        [PCX_TEXT_STRING_GAME_TYPE_REFORMATION] =
        "Reformation",
        [PCX_TEXT_STRING_GAME_TYPE_REFORMATION_INSPECTOR] =
        "Reformation + Inquisitor",
        [PCX_TEXT_STRING_COUP] =
        "Coup",
        [PCX_TEXT_STRING_INCOME] =
        "Income",
        [PCX_TEXT_STRING_FOREIGN_AID] =
        "Foreign aid",
        [PCX_TEXT_STRING_TAX] =
        "Tax (Duke)",
        [PCX_TEXT_STRING_CONVERT] =
        "Convert",
        [PCX_TEXT_STRING_EMBEZZLE] =
        "Embezzle",
        [PCX_TEXT_STRING_ASSASSINATE] =
        "Assassinate (Assassin)",
        [PCX_TEXT_STRING_EXCHANGE] =
        "Exchange (Ambassador)",
        [PCX_TEXT_STRING_EXCHANGE_INSPECTOR] =
        "Exchange (Inquisitor)",
        [PCX_TEXT_STRING_INSPECT] =
        "Interrogate (Inquisitor)",
        [PCX_TEXT_STRING_STEAL] =
        "Steal (Captain)",
        [PCX_TEXT_STRING_ACCEPT] =
        "Accept",
        [PCX_TEXT_STRING_CHALLENGE] =
        "Challenge",
        [PCX_TEXT_STRING_BLOCK] =
        "Block",
        [PCX_TEXT_STRING_1_COIN] =
        "1 coin",
        [PCX_TEXT_STRING_PLURAL_COINS] =
        "%i coins",
        [PCX_TEXT_STRING_YOUR_CARDS_ARE] =
        "Your cards are:",
        [PCX_TEXT_STRING_NOONE] =
        "Nobody",
        [PCX_TEXT_STRING_WON_1] =
        "%s won!",
        [PCX_TEXT_STRING_WON_PLURAL] =
        "%s won!",
        [PCX_TEXT_STRING_YOUR_GO] =
        "%s, it’s your turn. What do you want to do?",
        [PCX_TEXT_STRING_WHICH_CARD_TO_LOSE] =
        "Which card do you want to lose?",
        [PCX_TEXT_STRING_CHALLENGE_SUCCEEDED] =
        "%s challenged and %s didn’t have %s so %s loses a card.",
        [PCX_TEXT_STRING_INVERTED_CHALLENGE_SUCCEEDED] =
        "%s challenged and %s conceded so %s loses a card.",
        [PCX_TEXT_STRING_CHALLENGE_FAILED] =
        "%s challenged but %s did have %s. %s loses a card and %s "
        "receives a replacement from the deck.",
        [PCX_TEXT_STRING_INVERTED_CHALLENGE_FAILED] =
        "%s challenged and %s showed %s so %s changes their cards and "
        "%s loses a card.",
        [PCX_TEXT_STRING_CHOOSING_REVEAL] =
        "%s challenged and now %s is choosing which card to reveal.",
        [PCX_TEXT_STRING_CHOOSING_REVEAL_INVERTED] =
        "%s challenged and now %s is choosing whether to concede.",
        [PCX_TEXT_STRING_ANNOUNCE_CHALLENGE] =
        "%s doesn’t believe that you have %s.\n"
        "Which card do you want to show them?",
        [PCX_TEXT_STRING_ANNOUNCE_INVERTED_CHALLENGE] =
        "%s believes that you do have %s.\n"
        "Do you want to concede?",
        [PCX_TEXT_STRING_CONCEDE] =
        "Concede",
        [PCX_TEXT_STRING_SHOW_CARDS] =
        "Show cards",
        [PCX_TEXT_STRING_NO_CHALLENGE_SO_BLOCK] =
        "Nobody challenged. The action was blocked.",
        [PCX_TEXT_STRING_CLAIM_CARDS_TO_BLOCK] =
        "%s claims to have %s and blocks the action.",
        [PCX_TEXT_STRING_DOES_SOMEBODY_CHALLENGE] =
        "Does somebody want to challenge them?",
        [PCX_TEXT_STRING_OR_BLOCK_NO_TARGET] =
        "Or does somebody want to claim to have %s and block them?",
        [PCX_TEXT_STRING_BLOCK_NO_TARGET] =
        "Does somebody want to claim to have %s and block them?",
        [PCX_TEXT_STRING_OR_BLOCK_OTHER_ALLEGIANCE] =
        "Or does somebody from another allegiance want to claim to have %s "
        "and block them?",
        [PCX_TEXT_STRING_BLOCK_OTHER_ALLEGIANCE] =
        "Does somebody from another allegiance want to claim to have %s "
        "and block them?",
        [PCX_TEXT_STRING_OR_BLOCK_WITH_TARGET] =
        "Or %s, do you want to claim to have %s and block them?",
        [PCX_TEXT_STRING_BLOCK_WITH_TARGET] =
        "%s, do you want to claim to have %s and block them?",
        [PCX_TEXT_STRING_WHO_TO_COUP] =
        "%s, who do you want to kill during the coup?",
        [PCX_TEXT_STRING_DOING_COUP] =
        "💣 %s does a coup against %s",
        [PCX_TEXT_STRING_DOING_INCOME] =
        "💲 %s takes 1 coin of income",
        [PCX_TEXT_STRING_REALLY_DOING_FOREIGN_AID] =
        "Nobody blocked, %s takes the two coins",
        [PCX_TEXT_STRING_DOING_FOREIGN_AID] =
        "💴 %s receives 2 coins from foreign aid.",
        [PCX_TEXT_STRING_EMBEZZLING] =
        "💼 %s claims not to have the duke and embezzles the money in the "
        "treasury.",
        [PCX_TEXT_STRING_REALLY_EMBEZZLING] =
        "Nobody challenged, %s takes the money in the treasury.",
        [PCX_TEXT_STRING_REALLY_DOING_TAX] =
        "Nobody challenged, %s takes the 3 coins.",
        [PCX_TEXT_STRING_DOING_TAX] =
        "💸 %s claims to have the duke and takes 3 coins from tax.",
        [PCX_TEXT_STRING_WHO_TO_CONVERT] =
        "%s, who do you want to convert?",
        [PCX_TEXT_STRING_CONVERTS_SELF] =
        "%s pays 1 coin to the treasury and converts themself.",
        [PCX_TEXT_STRING_CONVERTS_SOMEONE_ELSE] =
        "%s pays 2 coins to the treasury and converts %s.",
        [PCX_TEXT_STRING_REALLY_DOING_ASSASSINATION] =
        "Nobody blocked or challenged, %s assassinates %s",
        [PCX_TEXT_STRING_SELECT_TARGET_ASSASSINATION] =
        "%s, who do you want to assassinate?",
        [PCX_TEXT_STRING_DOING_ASSASSINATION] =
        "🗡 %s wants to assassinate %s.",
        [PCX_TEXT_STRING_WHICH_CARDS_TO_KEEP] =
        "Which cards do you want to keep?",
        [PCX_TEXT_STRING_REALLY_DOING_EXCHANGE] =
        "Nobody challenged, %s exchanges cards.",
        [PCX_TEXT_STRING_DOING_EXCHANGE] =
        "🔄 %s claims to have the ambassador and wants to exchange cards.",
        [PCX_TEXT_STRING_DOING_EXCHANGE_INSPECTOR] =
        "🔄 %s claims to have the inquisitor and wants to exchange cards.",
        [PCX_TEXT_STRING_REALLY_DOING_INSPECT] =
        "Nobody challenged, %s chooses a card to show to %s",
        [PCX_TEXT_STRING_SELECT_TARGET_INSPECT] =
        "%s, who do you want to interrogate?",
        [PCX_TEXT_STRING_DOING_INSPECT] =
        "🔍 %s claims to have the inquisitor and wants to interrogate %s",
        [PCX_TEXT_STRING_CHOOSE_CARD_TO_SHOW] =
        "Which card do you want to show to %s?",
        [PCX_TEXT_STRING_OTHER_PLAYER_DECIDING_CAN_KEEP] =
        "%s is now deciding whether you can keep %s",
        [PCX_TEXT_STRING_SHOWING_CARD] =
        "%s shows %s to you. Can they keep it?",
        [PCX_TEXT_STRING_YES] =
        "Yes",
        [PCX_TEXT_STRING_NO] =
        "No",
        [PCX_TEXT_STRING_ALLOW_KEEP] =
        "%s let %s keep the card that they showed.",
        [PCX_TEXT_STRING_DONT_ALLOW_KEEP] =
        "%s made %s change the card that they showed.",
        [PCX_TEXT_STRING_REALLY_DOING_STEAL] =
        "Nobody blocked or challenged, %s steals from %s.",
        [PCX_TEXT_STRING_SELECT_TARGET_STEAL] =
        "%s, who do you want to steal from?",
        [PCX_TEXT_STRING_DOING_STEAL] =
        "💰 %s wants to steal from %s.",
        [PCX_TEXT_STRING_CHARACTER_NAME_DUKE] =
        "Duke",
        [PCX_TEXT_STRING_CHARACTER_NAME_ASSASSIN] =
        "Assassin",
        [PCX_TEXT_STRING_CHARACTER_NAME_CONTESSA] =
        "Contessa",
        [PCX_TEXT_STRING_CHARACTER_NAME_CAPTAIN] =
        "Captain",
        [PCX_TEXT_STRING_CHARACTER_NAME_AMBASSADOR] =
        "Ambassador",
        [PCX_TEXT_STRING_CHARACTER_NAME_INSPECTOR] =
        "Inquisitor",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_DUKE] =
        "the duke",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_ASSASSIN] =
        "the assassin",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CONTESSA] =
        "the contessa",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CAPTAIN] =
        "the captain",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_AMBASSADOR] =
        "the ambassador",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_INSPECTOR] =
        "the inquisitor",
        [PCX_TEXT_STRING_COINS_IN_TREASURY] =
        "Treasury: %i",
        [PCX_TEXT_STRING_ROLE_NAME_DRIVER] =
        "Driver",
        [PCX_TEXT_STRING_ROLE_NAME_LOCKPICK] =
        "Lockpick",
        [PCX_TEXT_STRING_ROLE_NAME_MUSCLE] =
        "Muscle",
        [PCX_TEXT_STRING_ROLE_NAME_CON_ARTIST] =
        "Con Artist",
        [PCX_TEXT_STRING_ROLE_NAME_LOOKOUT] =
        "Lookout",
        [PCX_TEXT_STRING_ROLE_NAME_SNITCH] =
        "Snitch",
        [PCX_TEXT_STRING_ROUND_NUM] =
        "Round %i / %i",
        [PCX_TEXT_STRING_CHOOSE_HEIST_DIFFICULTY] =
        "%s, you are the gang leader. How many specialists do you want in the "
        "heist?",
        [PCX_TEXT_STRING_HEIST_SIZE_CHOSEN] =
        "The heist will need the following %i specialists:",
        [PCX_TEXT_STRING_DISCUSS_HEIST] =
        "Now you can discuss amongst yourselves about which specialist you "
        "will add to the heist. When you are ready, you can secretly choose "
        "your card.",
        [PCX_TEXT_STRING_CARDS_CHOSEN] =
        "Everybody made their choice! The specialists are:",
        [PCX_TEXT_STRING_NEEDED_CARDS_WERE] =
        "The people needed were:",
        [PCX_TEXT_STRING_YOU_CHOSE] =
        "You chose:",
        [PCX_TEXT_STRING_WHICH_ROLE] =
        "Which card do you want to choose?",
        [PCX_TEXT_STRING_HEIST_SUCCESS] =
        "The heist was a success! Every player that didn’t choose the snitch "
        "receives %i coins.",
        [PCX_TEXT_STRING_HEIST_FAILED] =
        "The heist failed! Everybody who didn’t choose the snitch loses 1 "
        "coin.",
        [PCX_TEXT_STRING_SNITCH_GAIN_1] =
        "Everyone else gains 1 coin.",
        [PCX_TEXT_STRING_SNITCH_GAIN_PLURAL] =
        "Everyone else gains %i coins.",
        [PCX_TEXT_STRING_EVERYONE_SNITCHED] =
        "The heist failed and everybody snitched! Nobody gains anything.",
        [PCX_TEXT_STRING_NOONE_SNITCHED] =
        "Nobody snitched.",
        [PCX_TEXT_STRING_1_SNITCH] =
        "1 snitch",
        [PCX_TEXT_STRING_PLURAL_SNITCHES] =
        "%i snitches",
        [PCX_TEXT_STRING_GUARD] =
        "Guard",
        [PCX_TEXT_STRING_GUARD_OBJECT] =
        "the guard",
        [PCX_TEXT_STRING_GUARD_DESCRIPTION] =
        "Name a card other than the guard and choose a player. "
        "If that player has that card, they lose the round.",
        [PCX_TEXT_STRING_SPY] =
        "Spy",
        [PCX_TEXT_STRING_SPY_OBJECT] =
        "the spy",
        [PCX_TEXT_STRING_SPY_DESCRIPTION] =
        "Look at another player’s hand.",
        [PCX_TEXT_STRING_BARON] =
        "Baron",
        [PCX_TEXT_STRING_BARON_OBJECT] =
        "the baron",
        [PCX_TEXT_STRING_BARON_DESCRIPTION] =
        "Secretly compare hands with another player. The person who has "
        "the lowest valued card loses the round.",
        [PCX_TEXT_STRING_HANDMAID] =
        "Handmaid",
        [PCX_TEXT_STRING_HANDMAID_OBJECT] =
        "the handmaid",
        [PCX_TEXT_STRING_HANDMAID_DESCRIPTION] =
        "Until your next turn, ignore any effects from other player’s cards.",
        [PCX_TEXT_STRING_PRINCE] =
        "Prince",
        [PCX_TEXT_STRING_PRINCE_OBJECT] =
        "the prince",
        [PCX_TEXT_STRING_PRINCE_DESCRIPTION] =
        "Choose a player (can be yourself) who will discard their hand and "
        "take a new card.",
        [PCX_TEXT_STRING_KING] =
        "King",
        [PCX_TEXT_STRING_KING_OBJECT] =
        "the king",
        [PCX_TEXT_STRING_KING_DESCRIPTION] =
        "Choose another player and exchange hands with them.",
        [PCX_TEXT_STRING_COMTESSE] =
        "Comtesse",
        [PCX_TEXT_STRING_COMTESSE_OBJECT] =
        "the comtesse",
        [PCX_TEXT_STRING_COMTESSE_DESCRIPTION] =
        "If you have this card along with the king or the prince in your hand "
        "you must discard this card.",
        [PCX_TEXT_STRING_PRINCESS] =
        "Princess",
        [PCX_TEXT_STRING_PRINCESS_OBJECT] =
        "the princess",
        [PCX_TEXT_STRING_PRINCESS_DESCRIPTION] =
        "If you discard this card you lose the round.",
        [PCX_TEXT_STRING_ONE_COPY] =
        "1 copy",
        [PCX_TEXT_STRING_PLURAL_COPIES] =
        "%i copies",
        [PCX_TEXT_STRING_YOUR_CARD_IS] =
        "Your card is: ",
        [PCX_TEXT_STRING_VISIBLE_CARDS] =
        "Discarded cards: ",
        [PCX_TEXT_STRING_N_CARDS] =
        "Deck: ",
        [PCX_TEXT_STRING_YOUR_GO_NO_QUESTION] =
        "<b>%p</b>, it’s your go.",
        [PCX_TEXT_STRING_DISCARD_WHICH_CARD] =
        "Which card do you want to discard?",
        [PCX_TEXT_STRING_EVERYONE_PROTECTED] =
        "%p discards %C but all the other players are protected and it has no "
        "effect.",
        [PCX_TEXT_STRING_WHO_GUESS] =
        "Whose card do you want to guess?",
        [PCX_TEXT_STRING_GUESS_WHICH_CARD] =
        "Which card do you want to guess?",
        [PCX_TEXT_STRING_GUARD_SUCCESS] =
        "%p discarded %C and correctly guessed that %p had %C. %p loses the "
        "round.",
        [PCX_TEXT_STRING_GUARD_FAIL] =
        "%p discarded %C and incorrectly guessed that %p had %C.",
        [PCX_TEXT_STRING_WHO_SEE_CARD] =
        "Whose card do you want to see?",
        [PCX_TEXT_STRING_SHOWS_CARD] =
        "%p discarded %C and made %p secretly show their card.",
        [PCX_TEXT_STRING_TELL_SPIED_CARD] =
        "%p has %C",
        [PCX_TEXT_STRING_WHO_COMPARE] =
        "Who do you want to compare cards with?",
        [PCX_TEXT_STRING_COMPARE_LOSER] =
        "%p discarded %C and compared their hand with %p. %p had the lower "
        "card and loses the round.",
        [PCX_TEXT_STRING_TELL_COMPARE] =
        "You have %C and %p has %C",
        [PCX_TEXT_STRING_COMPARE_CARDS_EQUAL] =
        "%p discarded %C and compared their hand with %p. The cards were equal "
        "and nobody loses the round.",
        [PCX_TEXT_STRING_DISCARDS_HANDMAID] =
        "%p discarded %C and will be protected until their next turn",
        [PCX_TEXT_STRING_WHO_PRINCE] =
        "Who do you want to make discard their hand?",
        [PCX_TEXT_STRING_PRINCE_SELF] =
        "%p discarded %C and made themself discard %C",
        [PCX_TEXT_STRING_PRINCE_OTHER] =
        "%p discarded %C and made %p discard %C",
        [PCX_TEXT_STRING_FORCE_DISCARD_PRINCESS] =
        " and so they lose the round.",
        [PCX_TEXT_STRING_FORCE_DISCARD_OTHER] =
        " and take a new card.",
        [PCX_TEXT_STRING_WHO_EXCHANGE] =
        "Who do you want to exchange hands with?",
        [PCX_TEXT_STRING_TELL_EXCHANGE] =
        "You gave away %C to %p and received %C",
        [PCX_TEXT_STRING_EXCHANGES] =
        "%p discarded the king and exchanges hands with %p.",
        [PCX_TEXT_STRING_DISCARDS_COMTESSE] =
        "%p discarded %C",
        [PCX_TEXT_STRING_DISCARDS_PRINCESS] =
        "%p discarded %C and lost the round.",
        [PCX_TEXT_STRING_EVERYBODY_SHOWS_CARD] =
        "The round has finished and everybody shows their card:",
        [PCX_TEXT_STRING_SET_ASIDE_CARD] =
        "The hidden card was %c",
        [PCX_TEXT_STRING_WINS_ROUND] =
        "💘 %p wins the round and gains a point of affection from the princess.",
        [PCX_TEXT_STRING_WINS_PRINCESS] =
        "🏆 %p has %i points of affection and wins the game!",
        [PCX_TEXT_STRING_FIGHTERS_ARE] =
        "The players in the next fight are:\n"
        "\n"
        "%s\n"
        "%s\n"
        "\n"
        "They are now choosing their fighters.",
        [PCX_TEXT_STRING_POSSIBLE_ROLES] =
        "Your characters are:",
        [PCX_TEXT_STRING_POSSIBLE_ATTRIBUTES] =
        "Your attributes are:",
        [PCX_TEXT_STRING_CHOOSE_ROLE] =
        "Please choose a character.",
        [PCX_TEXT_STRING_CHOOSE_ATTRIBUTE] =
        "Please choose an attribute.",
        [PCX_TEXT_STRING_YOUR_FIGHTER_IS] =
        "Thanks. Your fighter is:",
        [PCX_TEXT_STRING_FIGHTERS_CHOSEN] =
        "The fighters are ready! They are:",
        [PCX_TEXT_STRING_NOW_ARGUE] =
        "You both now have to argue why your fighter would win in a "
        "fight to the death. Go!",
        [PCX_TEXT_STRING_DONT_FORGET_TO_VOTE] =
        "Don’t forget to vote! The current votes are:",
        [PCX_TEXT_STRING_YOU_CAN_VOTE] =
        "Has the arguing already finished? The other players can now vote "
        "with the buttons below or wait for the debate to finish.",
        [PCX_TEXT_STRING_X_VOTED_Y] =
        "%s voted for %s",
        [PCX_TEXT_STRING_CURRENT_VOTES_ARE] =
        "The current votes are:",
        [PCX_TEXT_STRING_FIGHT_EQUAL_RESULT] =
        "It’s a draw! There will now be a decider fight without "
        "attributes.",
        [PCX_TEXT_STRING_FIGHT_WINNER_IS] =
        "%s won the fight! The current points are:",
        [PCX_TEXT_STRING_STAYS_ON] =
        "The first player to reach %i points wins the game. "
        "%s will stay on for the next fight without changing their cards.",
        [PCX_TEXT_STRING_THROW] =
        "Throw dice",
        [PCX_TEXT_STRING_STOP] =
        "Stop",
        [PCX_TEXT_STRING_STOP_SCORE] =
        "%p stops and adds %i to their score.",
        [PCX_TEXT_STRING_THROW_FIRST_DICE] =
        "<b>%p</b>, it’s your go, press the button to roll the dice.",
        [PCX_TEXT_STRING_YOUR_DICE_ARE] =
        "Your dice are:",
        [PCX_TEXT_STRING_THROWING_DICE] =
        "Throwing dice…",
        [PCX_TEXT_STRING_SCORE_SO_FAR] =
        "Score so far:",
        [PCX_TEXT_STRING_DICE_IN_HAND] =
        "In your hand:",
        [PCX_TEXT_STRING_NO_DICE_IN_HAND] =
        "nothing",
        [PCX_TEXT_STRING_REMAINING_DICE_IN_BOX] =
        "Dice in box:",
        [PCX_TEXT_STRING_YOU_ARE_DEAD] =
        "You got shot too many times and lose all of your points from this "
        "turn!",
        [PCX_TEXT_STRING_THROW_OR_STOP] =
        "Do you want to throw the dice again or stop now?",
        [PCX_TEXT_STRING_START_LAST_ROUND] =
        "%p has reached %i points so this will be the final round",
        [PCX_TEXT_STRING_WINS] =
        "🏆 <b>%p</b> wins!",
        [PCX_TEXT_STRING_FINAL_SCORES] =
        "The final scores are:",
        [PCX_TEXT_STRING_EVERYBODY_CHOOSE_CARD] =
        "Everybody now has to choose a card to play.",
        [PCX_TEXT_STRING_WHICH_CARD_TO_PLAY] =
        "Which card do you want to play?",
        [PCX_TEXT_STRING_CARD_CHOSEN] =
        "You chose:",
        [PCX_TEXT_STRING_CHOSEN_CARDS_ARE] =
        "Everybody has chosen! The cards are:",
        [PCX_TEXT_STRING_ADDED_TO_ROW] =
        "%s adds their card to row %c.",
        [PCX_TEXT_STRING_ROW_FULL] =
        "The row is full so they have to take it and add %i 🐮 to their score.",
        [PCX_TEXT_STRING_CHOOSE_ROW] =
        "%s, your card is lower than all of the rows. You have to choose a row "
        "and take it.",
        [PCX_TEXT_STRING_CHOSEN_ROW] =
        "%s takes row %c and adds %i 🐮 to their score.",
        [PCX_TEXT_STRING_ROUND_OVER] =
        "The round is over and the scores are now:",
        [PCX_TEXT_STRING_END_POINTS] =
        "%s has at least %i points and ends the game.",
        [PCX_TEXT_STRING_WINS_PLAIN] =
        "🏆 %s wins!",
        [PCX_TEXT_STRING_YOU_ARE_LEADER] =
        "%s leads the trick.",
        [PCX_TEXT_STRING_PLAYER_PLAYED] =
        "%s played:",
        [PCX_TEXT_STRING_FOLLOW_PLAYER] =
        "Now %s is choosing which card to play.",
        [PCX_TEXT_STRING_PLAYED_THREE] =
        "Now they are choosing whether to change the decree card.",
        [PCX_TEXT_STRING_PLAYED_FIVE] =
        "Now they will take a card from the deck and discard one.",
        [PCX_TEXT_STRING_TRICK_WINNER] =
        "%s won the trick.",
        [PCX_TEXT_STRING_TRICKS_IN_ROUND_ARE] =
        "The tricks won in this round so far are:",
        [PCX_TEXT_STRING_WIN_TRICK_SEVEN] =
        "They immediately win a point because of the 7 card.",
        [PCX_TEXT_STRING_WIN_TRICK_TWO_SEVENS] =
        "They immediately win two points because of the two 7’s.",
        [PCX_TEXT_STRING_YOU_DREW] =
        "You draw:",
        [PCX_TEXT_STRING_WHICH_CARD_DISCARD] =
        "Which card do you want to discard?",
        [PCX_TEXT_STRING_TRUMP_CARD_IS] =
        "The decree card is:",
        [PCX_TEXT_STRING_WHICH_CARD_EXCHANGE] =
        "Which card do you want to be the decree card?",
        [PCX_TEXT_STRING_DONT_EXCHANGE] =
        "Don’t change the card",
        [PCX_TEXT_STRING_DOESNT_EXCHANGE] =
        "%s decided not to change the decree card.",
        [PCX_TEXT_STRING_WORDS_ARE] =
        "The word list is:",
        [PCX_TEXT_STRING_SECRET_WORD_IS] =
        "The secret word is:",
        [PCX_TEXT_STRING_YOU_ARE_THE_CHAMELEON] =
        "You are the chameleon 🦎",
        [PCX_TEXT_STRING_CLUE_QUESTION] =
        "%p, please type your clue.",
        [PCX_TEXT_STRING_START_DEBATE] =
        "Now you need to discuss who you think the chameleon is. "
        "When you are ready you can vote.",
        [PCX_TEXT_STRING_YOU_CAN_VOTE_FOR_A_PLAYER] =
        "If you’ve finished the discussion, you can vote for who you think "
        "the chameleon is.",
        [PCX_TEXT_STRING_PLAYER_VOTED] =
        "%s voted",
        [PCX_TEXT_STRING_EVERYBODY_VOTED] =
        "Everybody voted!",
        [PCX_TEXT_STRING_ITS_A_DRAW] =
        "It’s a draw! %p has the deciding vote.",
        [PCX_TEXT_STRING_CHOSEN_PLAYER] =
        "The chosen player is %p.",
        [PCX_TEXT_STRING_YOU_FOUND_THE_CHAMELEON] =
        "You found the chameleon! 🦎",
        [PCX_TEXT_STRING_YOU_DIDNT_FIND_THE_CHAMELEON] =
        "That’s not the chameleon!",
        [PCX_TEXT_STRING_CHAMELEON_WINS_POINTS] =
        "%p gets two points and everybody else gets nothing.",
        [PCX_TEXT_STRING_SCORES] =
        "Scores:",
        [PCX_TEXT_STRING_NOW_GUESS] =
        "%p, now try to guess the secret word.",
        [PCX_TEXT_STRING_CHAMELEON_GUESSED] =
        "The chameleon guessed %p.",
        [PCX_TEXT_STRING_CORRECT_GUESS] =
        "That’s the right word!",
        [PCX_TEXT_STRING_CORRECT_WORD_IS] =
        "The real secret word is %p.",
        [PCX_TEXT_STRING_SECRET_WORD_WAS] =
        "The secret word was %p.",
        [PCX_TEXT_STRING_ESCAPED_SCORE] =
        "%p gets one point and everybody else gets nothing.",
        [PCX_TEXT_STRING_CAUGHT_SCORE] =
        "Everybody except %p gets 2 points.",
        [PCX_TEXT_STRING_START_ROUND_BUTTON] =
        "Start the next round",
};

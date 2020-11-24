/*
 * Puxcobot - A robot to play Coup in Esperanto (Puĉo)
 * Copyright (C) 2020  Neil Roberts
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
pcx_text_chinese_traditional[] = {
        [PCX_TEXT_STRING_LANGUAGE_CODE] =
        "zh-tw",
        [PCX_TEXT_STRING_NAME_COUP] =
        "政變",
        [PCX_TEXT_STRING_NAME_SNITCH] =
        "Snitch",
        [PCX_TEXT_STRING_NAME_LOVE] =
        "情書",
        [PCX_TEXT_STRING_NAME_SIX] =
        "誰是牛頭王",
        [PCX_TEXT_STRING_NAME_SUPERFIGHT] =
        "Superfight",
        [PCX_TEXT_STRING_NAME_ZOMBIE] =
        "僵屍骰",
        [PCX_TEXT_STRING_COUP_START_COMMAND] =
        "/coup",
        [PCX_TEXT_STRING_COUP_START_COMMAND_DESCRIPTION] =
        "開政變的遊戲",
        [PCX_TEXT_STRING_SNITCH_START_COMMAND] =
        "/snitch",
        [PCX_TEXT_STRING_SNITCH_START_COMMAND_DESCRIPTION] =
        "開 Snitch 的遊戲",
        [PCX_TEXT_STRING_LOVE_START_COMMAND] =
        "/letter",
        [PCX_TEXT_STRING_LOVE_START_COMMAND_DESCRIPTION] =
        "開情書的遊戲",
        [PCX_TEXT_STRING_SIX_START_COMMAND] =
        "/six",
        [PCX_TEXT_STRING_SIX_START_COMMAND_DESCRIPTION] =
        "開誰是牛頭王的遊戲",
        [PCX_TEXT_STRING_SUPERFIGHT_START_COMMAND] =
        "/superfight",
        [PCX_TEXT_STRING_SUPERFIGHT_START_COMMAND_DESCRIPTION] =
        "開 Superfight 的遊戲",
        [PCX_TEXT_STRING_ZOMBIE_START_COMMAND] =
        "/zombie",
        [PCX_TEXT_STRING_ZOMBIE_START_COMMAND_DESCRIPTION] =
        "開殭屍骰的遊戲",
        [PCX_TEXT_STRING_WHICH_HELP] =
        "你想了解哪款遊戲？",
        [PCX_TEXT_STRING_TIMEOUT_START] =
        "若沒人在 %i 分鐘內加入，遊戲就會立刻開始。",
        [PCX_TEXT_STRING_TIMEOUT_ABANDON] =
        "此遊戲已經至少 %i 分鐘沒動了，即將關閉。",
        [PCX_TEXT_STRING_NEED_PUBLIC_GROUP] =
        "要玩遊戲，請將機器人加到群組中，然後在那裡開始遊戲。",
        [PCX_TEXT_STRING_SEND_PRIVATE_MESSAGE] =
        "在加入遊戲前，你需要先點擊 @%s 並傳訊息，我才有權限可以用私訊告訴你卡牌。在你完成"
        "後你才可以再回來這按加入遊戲。",
        [PCX_TEXT_STRING_ALREADY_IN_GAME] =
        "你已經加入遊戲了。",
        [PCX_TEXT_STRING_ALREADY_GAME] =
        "這個群組已經有一個遊戲了。",
        [PCX_TEXT_STRING_WHICH_GAME] =
        "你想玩哪款遊戲？",
        [PCX_TEXT_STRING_GAME_FULL] =
        "此遊戲滿人了！",
        [PCX_TEXT_STRING_GAME_ALREADY_STARTED] =
        "此遊戲已經開始了。",
        [PCX_TEXT_STRING_NO_GAME] =
        "目前這個群組沒有遊戲。",
        [PCX_TEXT_STRING_CANCELED] =
        "遊戲已取消。",
        [PCX_TEXT_STRING_CANT_CANCEL] =
        "只有在遊戲中的玩家可以取消遊戲。",
        [PCX_TEXT_STRING_NAME_FROM_ID] =
        "Mx.%i",
        [PCX_TEXT_STRING_FINAL_CONJUNCTION] =
        " 及 ",
        [PCX_TEXT_STRING_FINAL_DISJUNCTION] =
        " 或 ",
        [PCX_TEXT_STRING_WELCOME] =
        "歡迎。其他玩家可以輸入 /join 加入遊戲或你可以輸入 /start 開始遊戲。",
        [PCX_TEXT_STRING_WELCOME_FULL] =
        "歡迎。遊戲現在已滿人，將立刻開始。",
        [PCX_TEXT_STRING_WELCOME_BUTTONS] =
        "%s 加入了遊戲。你可以等待更多玩家或點擊下方的按鈕開始。",
        [PCX_TEXT_STRING_WELCOME_BUTTONS_TOO_FEW] =
        "%s 加入了遊戲。等待更多的玩家，再開始遊戲",
        [PCX_TEXT_STRING_WELCOME_BUTTONS_FULL] =
        "%s 加入了遊戲。遊戲現在已滿人，將立刻開始。",
        [PCX_TEXT_STRING_PLAYER_LEFT] =
        "%s 離開了",
        [PCX_TEXT_STRING_START_BUTTON] =
        "開始",
        [PCX_TEXT_STRING_CHOSEN_GAME] =
        "遊戲：%s",
        [PCX_TEXT_STRING_CURRENT_PLAYERS] =
        "目前的玩家有：",
        [PCX_TEXT_STRING_JOIN_BEFORE_START] =
        "在嘗試開始遊戲前，請用 /join 加入遊戲。",
        [PCX_TEXT_STRING_NEED_MIN_PLAYERS] =
        "至少要 %i 位玩家才能玩。",
        [PCX_TEXT_STRING_JOIN_COMMAND] =
        "/join",
        [PCX_TEXT_STRING_JOIN_COMMAND_DESCRIPTION] =
        "加入現有的遊戲或開一個新的。",
        [PCX_TEXT_STRING_START_COMMAND] =
        "/start",
        [PCX_TEXT_STRING_START_COMMAND_DESCRIPTION] =
        "開始現有的遊戲",
        [PCX_TEXT_STRING_CANCEL_COMMAND] =
        "/cancel",
        [PCX_TEXT_STRING_CANCEL_COMMAND_DESCRIPTION] =
        "取消現有的遊戲",
        [PCX_TEXT_STRING_HELP_COMMAND] =
        "/help",
        [PCX_TEXT_STRING_HELP_COMMAND_DESCRIPTION] =
        "顯示規則簡介",
        [PCX_TEXT_STRING_RECEIVED_PRIVATE_MESSAGE] =
        "感謝私訊。你現在可以在群組加入遊戲了。",
        [PCX_TEXT_STRING_CHOOSE_GAME_TYPE] =
        "請選擇你想玩的遊戲變體。",
        [PCX_TEXT_STRING_GAME_TYPE_CHOSEN] =
        "選擇的遊戲變體是：%s",
        [PCX_TEXT_STRING_GAME_TYPE_ORIGINAL] =
        "原版",
        [PCX_TEXT_STRING_GAME_TYPE_INSPECTOR] =
        "Inquisitor",
        [PCX_TEXT_STRING_GAME_TYPE_REFORMATION] =
        "Reformation",
        [PCX_TEXT_STRING_GAME_TYPE_REFORMATION_INSPECTOR] =
        "Reformation + Inquisitor",
        [PCX_TEXT_STRING_COUP] =
        "政變",
        [PCX_TEXT_STRING_INCOME] =
        "收入",
        [PCX_TEXT_STRING_FOREIGN_AID] =
        "贊助",
        [PCX_TEXT_STRING_TAX] =
        "收稅（公爵）",
        [PCX_TEXT_STRING_CONVERT] =
        "翻面",
        [PCX_TEXT_STRING_EMBEZZLE] =
        "貪汙",
        [PCX_TEXT_STRING_ASSASSINATE] =
        "暗殺（刺客）",
        [PCX_TEXT_STRING_EXCHANGE] =
        "交換（大使）",
        [PCX_TEXT_STRING_EXCHANGE_INSPECTOR] =
        "交換 (審訊官)",
        [PCX_TEXT_STRING_INSPECT] =
        "審問 (審訊官)",
        [PCX_TEXT_STRING_STEAL] =
        "偷竊 (上尉)",
        [PCX_TEXT_STRING_ACCEPT] =
        "同意",
        [PCX_TEXT_STRING_CHALLENGE] =
        "質疑",
        [PCX_TEXT_STRING_BLOCK] =
        "阻止",
        [PCX_TEXT_STRING_1_COIN] =
        "1 枚硬幣",
        [PCX_TEXT_STRING_PLURAL_COINS] =
        "%i 枚硬幣",
        [PCX_TEXT_STRING_YOUR_CARDS_ARE] =
        "你的卡牌有：",
        [PCX_TEXT_STRING_NOONE] =
        "沒人",
        [PCX_TEXT_STRING_WON_1] =
        "%s 贏了！",
        [PCX_TEXT_STRING_WON_PLURAL] =
        "%s 贏了！",
        [PCX_TEXT_STRING_YOUR_GO] =
        "%s，輪到你的回合。你想做什麼？",
        [PCX_TEXT_STRING_WHICH_CARD_TO_LOSE] =
        "你想丟哪張牌？",
        [PCX_TEXT_STRING_CHALLENGE_SUCCEEDED] =
        "%s 質疑了且 %s 沒有 %s 所以 %s 失去一張牌。",
        [PCX_TEXT_STRING_INVERTED_CHALLENGE_SUCCEEDED] =
        "%s 質疑了且 %s 承認了所以 %s 失去一張牌。",
        [PCX_TEXT_STRING_CHALLENGE_FAILED] =
        "%s 質疑了但 %s 有 %s。%s 失去一張牌且 %s 從牌庫換取一張新的牌。",
        [PCX_TEXT_STRING_INVERTED_CHALLENGE_FAILED] =
        "%s 質疑了且 %s 展示了 %s 所以 %s 換了他們的牌且 %s 失去一張牌。",
        [PCX_TEXT_STRING_CHOOSING_REVEAL] =
        "%s 質疑了且現在 %s 正在選擇要展示的牌。",
        [PCX_TEXT_STRING_CHOOSING_REVEAL_INVERTED] =
        "%s 質疑了且現在 %s 選擇是否讓步。",
        [PCX_TEXT_STRING_ANNOUNCE_CHALLENGE] =
        "%s 不相信你有 %s。\n"
        "你想展示哪張牌給他們。?",
        [PCX_TEXT_STRING_ANNOUNCE_INVERTED_CHALLENGE] =
        "%s 相信你有 %s。\n"
        "你想讓步嗎？",
        [PCX_TEXT_STRING_CONCEDE] =
        "讓步",
        [PCX_TEXT_STRING_SHOW_CARDS] =
        "展示手牌",
        [PCX_TEXT_STRING_NO_CHALLENGE_SO_BLOCK] =
        "沒人質疑。此行動被阻止。",
        [PCX_TEXT_STRING_CLAIM_CARDS_TO_BLOCK] =
        "%s 自稱有 %s 想阻止行動。",
        [PCX_TEXT_STRING_DOES_SOMEBODY_CHALLENGE] =
        "有人要質疑他們嗎？",
        [PCX_TEXT_STRING_OR_BLOCK_NO_TARGET] =
        "還是有人想聲稱自己擁有 %s 並阻止他們？",
        [PCX_TEXT_STRING_BLOCK_NO_TARGET] =
        "有人要聲稱擁有 %s 並阻止他們嗎？",
        [PCX_TEXT_STRING_OR_BLOCK_OTHER_ALLEGIANCE] =
        "還是另一個效忠的人想聲稱擁有 %s 並阻止他們？",
        [PCX_TEXT_STRING_BLOCK_OTHER_ALLEGIANCE] =
        "另一個效忠的人是否想聲稱擁有 %s 並阻止他們？",
        [PCX_TEXT_STRING_OR_BLOCK_WITH_TARGET] =
        "Or %s, do you want to claim to have %s and block them?",
        [PCX_TEXT_STRING_BLOCK_WITH_TARGET] =
        "%s, do you want to claim to have %s and block them?",
        [PCX_TEXT_STRING_WHO_TO_COUP] =
        "%s, who do you want to kill during the coup?",
        [PCX_TEXT_STRING_DOING_COUP] =
        "💣 %s 政變了 %s",
        [PCX_TEXT_STRING_DOING_INCOME] =
        "💲 %s 拿了 1 塊收入。",
        [PCX_TEXT_STRING_REALLY_DOING_FOREIGN_AID] =
        "Nobody blocked, %s takes the two coins",
        [PCX_TEXT_STRING_DOING_FOREIGN_AID] =
        "💴 %s 拿了 2 塊贊助。",
        [PCX_TEXT_STRING_EMBEZZLING] =
        "💼 %s 聲稱沒有公爵，並貪汙國庫裡的硬幣。",
        [PCX_TEXT_STRING_REALLY_EMBEZZLING] =
        "沒人質疑，%s 貪走了國庫裡的硬幣。",
        [PCX_TEXT_STRING_REALLY_DOING_TAX] =
        "沒人質疑，%s 拿了 3 枚硬幣。",
        [PCX_TEXT_STRING_DOING_TAX] =
        "💸 %s 自稱擁有公爵，想收 3 枚硬幣的稅。",
        [PCX_TEXT_STRING_WHO_TO_CONVERT] =
        "%s，你想把誰翻面？",
        [PCX_TEXT_STRING_CONVERTS_SELF] =
        "%s 付了 1 枚硬幣到國庫並把自己翻面。",
        [PCX_TEXT_STRING_CONVERTS_SOMEONE_ELSE] =
        "%s 付了 2 枚硬幣到國庫並把 %s 翻面。",
        [PCX_TEXT_STRING_REALLY_DOING_ASSASSINATION] =
        "沒人阻止或質疑 %s 暗殺了 %s",
        [PCX_TEXT_STRING_SELECT_TARGET_ASSASSINATION] =
        "%s，你想暗殺誰？",
        [PCX_TEXT_STRING_DOING_ASSASSINATION] =
        "🗡 %s 想暗殺 %s.",
        [PCX_TEXT_STRING_WHICH_CARDS_TO_KEEP] =
        "你想保留哪些牌？",
        [PCX_TEXT_STRING_REALLY_DOING_EXCHANGE] =
        "沒人質疑，%s 交換手牌。",
        [PCX_TEXT_STRING_DOING_EXCHANGE] =
        "🔄 %s 自稱有大使，想換牌。",
        [PCX_TEXT_STRING_DOING_EXCHANGE_INSPECTOR] =
        "🔄 %s 自稱有審判官，想換牌。",
        [PCX_TEXT_STRING_REALLY_DOING_INSPECT] =
        "沒人質疑，%s 選擇一張牌展示給 %s",
        [PCX_TEXT_STRING_SELECT_TARGET_INSPECT] =
        "%s，你想審問誰？",
        [PCX_TEXT_STRING_DOING_INSPECT] =
        "🔍 %s 自稱有審判官，想審問 %s",
        [PCX_TEXT_STRING_CHOOSE_CARD_TO_SHOW] =
        "你想展示哪張牌給 %s？",
        [PCX_TEXT_STRING_OTHER_PLAYER_DECIDING_CAN_KEEP] =
        "%s 正在決定是否可以保留 %s",
        [PCX_TEXT_STRING_SHOWING_CARD] =
        "%s 展示了 %s 給你。它們可以保留嗎？",
        [PCX_TEXT_STRING_YES] =
        "是",
        [PCX_TEXT_STRING_NO] =
        "否",
        [PCX_TEXT_STRING_ALLOW_KEEP] =
        "%s 讓 %s 保留牌他們展示的牌。",
        [PCX_TEXT_STRING_DONT_ALLOW_KEEP] =
        "%s 讓 %s 變更他們展示的牌。",
        [PCX_TEXT_STRING_REALLY_DOING_STEAL] =
        "沒人阻止或質疑，%s 偷了 %s 的錢。",
        [PCX_TEXT_STRING_SELECT_TARGET_STEAL] =
        "%s，你想偷誰？",
        [PCX_TEXT_STRING_DOING_STEAL] =
        "💰 %s 想偷 %s 的錢。",
        [PCX_TEXT_STRING_CHARACTER_NAME_DUKE] =
        "公爵",
        [PCX_TEXT_STRING_CHARACTER_NAME_ASSASSIN] =
        "刺客",
        [PCX_TEXT_STRING_CHARACTER_NAME_CONTESSA] =
        "女伯爵",
        [PCX_TEXT_STRING_CHARACTER_NAME_CAPTAIN] =
        "上尉",
        [PCX_TEXT_STRING_CHARACTER_NAME_AMBASSADOR] =
        "大使",
        [PCX_TEXT_STRING_CHARACTER_NAME_INSPECTOR] =
        "審判官",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_DUKE] =
        "公爵",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_ASSASSIN] =
        "刺客",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CONTESSA] =
        "女伯爵",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CAPTAIN] =
        "上尉",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_AMBASSADOR] =
        "大使",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_INSPECTOR] =
        "審判官",
        [PCX_TEXT_STRING_COINS_IN_TREASURY] =
        "國庫：%i",
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
        "守衛",
        [PCX_TEXT_STRING_GUARD_OBJECT] =
        "守衛",
        [PCX_TEXT_STRING_GUARD_DESCRIPTION] =
        "宣告另一位玩家的一張非守衛的牌。"
        "如果那位玩家有那張牌，他這回合就輸了。",
        [PCX_TEXT_STRING_SPY] =
        "密探",
        [PCX_TEXT_STRING_SPY_OBJECT] =
        "密探",
        [PCX_TEXT_STRING_SPY_DESCRIPTION] =
        "查看別人的手牌。",
        [PCX_TEXT_STRING_BARON] =
        "男爵",
        [PCX_TEXT_STRING_BARON_OBJECT] =
        "男爵",
        [PCX_TEXT_STRING_BARON_DESCRIPTION] =
        "暗中與另一位玩家比較。擁有最低分牌的人輸掉了這回合。",
        [PCX_TEXT_STRING_HANDMAID] =
        "侍女",
        [PCX_TEXT_STRING_HANDMAID_OBJECT] =
        "侍女",
        [PCX_TEXT_STRING_HANDMAID_DESCRIPTION] =
        "在下一輪之前，忽略其他玩家卡牌的任何影響。",
        [PCX_TEXT_STRING_PRINCE] =
        "王子",
        [PCX_TEXT_STRING_PRINCE_OBJECT] =
        "王子",
        [PCX_TEXT_STRING_PRINCE_DESCRIPTION] =
        "選擇一個玩家（可以是你自己）棄掉他們的手牌並抽一張新的。",
        [PCX_TEXT_STRING_KING] =
        "國王",
        [PCX_TEXT_STRING_KING_OBJECT] =
        "國王",
        [PCX_TEXT_STRING_KING_DESCRIPTION] =
        "選擇另一個玩家並與他交換手牌。",
        [PCX_TEXT_STRING_COMTESSE] =
        "女伯爵",
        [PCX_TEXT_STRING_COMTESSE_OBJECT] =
        "女伯爵",
        [PCX_TEXT_STRING_COMTESSE_DESCRIPTION] =
        "如果您手中有這張牌以及國王或王子，則必須棄掉此牌。",
        [PCX_TEXT_STRING_PRINCESS] =
        "公主",
        [PCX_TEXT_STRING_PRINCESS_OBJECT] =
        "公主",
        [PCX_TEXT_STRING_PRINCESS_DESCRIPTION] =
        "如果你棄掉此牌，則此回合就輸了。",
        [PCX_TEXT_STRING_ONE_COPY] =
        "1 張",
        [PCX_TEXT_STRING_PLURAL_COPIES] =
        "%i 張",
        [PCX_TEXT_STRING_YOUR_CARD_IS] =
        "你的牌是：",
        [PCX_TEXT_STRING_VISIBLE_CARDS] =
        "棄掉的牌：",
        [PCX_TEXT_STRING_N_CARDS] =
        "牌庫：",
        [PCX_TEXT_STRING_YOUR_GO_NO_QUESTION] =
        "<b>%p</b>，輪到你了。",
        [PCX_TEXT_STRING_DISCARD_WHICH_CARD] =
        "您想棄掉哪張牌？",
        [PCX_TEXT_STRING_EVERYONE_PROTECTED] =
        "%p 棄掉了 %C 但是其他所有玩家都受到保護所以無效。",
        [PCX_TEXT_STRING_WHO_GUESS] =
        "您想猜誰的牌？",
        [PCX_TEXT_STRING_GUESS_WHICH_CARD] =
        "您想猜哪張牌？",
        [PCX_TEXT_STRING_GUARD_SUCCESS] =
        "%p 棄掉了 %C 並正確地猜到了 %p 有 %C。%p 輸了這回合。",
        [PCX_TEXT_STRING_GUARD_FAIL] =
        "%p 棄掉了 %C 猜錯了。%p 沒有 %C。",
        [PCX_TEXT_STRING_WHO_SEE_CARD] =
        "您想看誰的牌？",
        [PCX_TEXT_STRING_SHOWS_CARD] =
        "%p 棄掉了 %C 並讓 %p 偷偷展示他的牌。",
        [PCX_TEXT_STRING_TELL_SPIED_CARD] =
        "%p 有 %C",
        [PCX_TEXT_STRING_WHO_COMPARE] =
        "您想與誰的牌比較？",
        [PCX_TEXT_STRING_COMPARE_LOSER] =
        "%p 棄掉了 %C 並比較他們的牌 %p。%p 的牌較低，因此輸掉了這回合。",
        [PCX_TEXT_STRING_TELL_COMPARE] =
        "你有 %C 且 %p 有 %C",
        [PCX_TEXT_STRING_COMPARE_CARDS_EQUAL] =
        "%p 棄掉了 %C 並比較他們的牌 %p。牌是一樣的，平手。",
        [PCX_TEXT_STRING_DISCARDS_HANDMAID] =
        "%p 棄掉了 %C 並將受到保護，直到他的下一回合。",
        [PCX_TEXT_STRING_WHO_PRINCE] =
        "您想讓誰棄掉他的牌？",
        [PCX_TEXT_STRING_PRINCE_SELF] =
        "%p 棄掉了 %C 並自己棄掉 %C",
        [PCX_TEXT_STRING_PRINCE_OTHER] =
        "%p 棄掉了 %C 並讓 %p 棄掉 %C",
        [PCX_TEXT_STRING_FORCE_DISCARD_PRINCESS] =
        " 因此他輸了這回合。",
        [PCX_TEXT_STRING_FORCE_DISCARD_OTHER] =
        " 並抽一張新牌。",
        [PCX_TEXT_STRING_WHO_EXCHANGE] =
        "您想與誰換牌？",
        [PCX_TEXT_STRING_TELL_EXCHANGE] =
        "你放棄 %C 給 %p 並獲得了 %C",
        [PCX_TEXT_STRING_EXCHANGES] =
        "%p discarded the king and exchanges hands with %p.",
        [PCX_TEXT_STRING_DISCARDS_COMTESSE] =
        "%p 棄掉了 %C",
        [PCX_TEXT_STRING_DISCARDS_PRINCESS] =
        "%p 棄掉了 %C 並輸了這回合。",
        [PCX_TEXT_STRING_EVERYBODY_SHOWS_CARD] =
        "回合結束，每個人都展示手牌：",
        [PCX_TEXT_STRING_SET_ASIDE_CARD] =
        "隱藏的牌是 %c",
        [PCX_TEXT_STRING_WINS_ROUND] =
        "💘 %p 贏得了這回合，並從公主那裡獲得了一顆愛心。",
        [PCX_TEXT_STRING_WINS_PRINCESS] =
        "🏆 %p 獲得了 %i 顆愛心並贏得遊戲！",
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
};

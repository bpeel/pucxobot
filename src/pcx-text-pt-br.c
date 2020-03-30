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

#include "pcx-text-pt-br.h"

#include "pcx-text.h"

const char *
pcx_text_pt_br[] = {
        [PCX_TEXT_STRING_LANGUAGE_CODE] = "pt-br",
        [PCX_TEXT_STRING_NAME_COUP] =
        "Golpe",
        [PCX_TEXT_STRING_NAME_SNITCH] =
        "Informante",
        [PCX_TEXT_STRING_NAME_LOVE] =
        "Carta de amor",
        [PCX_TEXT_STRING_COUP_START_COMMAND] =
        "/golpe",
        [PCX_TEXT_STRING_SNITCH_START_COMMAND] =
        "/informante",
        [PCX_TEXT_STRING_LOVE_START_COMMAND] =
        "/carta",
        [PCX_TEXT_STRING_WHICH_HELP] =
        "Para qual jogo você quer ajuda?",
        [PCX_TEXT_STRING_TIMEOUT_START] =
        "Ninguém se juntou por pelo menos %i  minutos. O jogo vai começar "
        "imediatamente.",
        [PCX_TEXT_STRING_TIMEOUT_ABANDON] =
        "O jogo ficou inativo por pelo menos %i minutos e será "
        "cancelado.",
        [PCX_TEXT_STRING_NEED_PUBLIC_GROUP] =
        "Por favor, junte-se a um jogo em um grupo público.",
        [PCX_TEXT_STRING_SEND_PRIVATE_MESSAGE] =
        "Por favor, envie uma mensagem privada para @%s para que eu possa enviar suas "
        "cartas em privado.",
        [PCX_TEXT_STRING_ALREADY_IN_GAME] =
        "Você já está em um jogo.",
        [PCX_TEXT_STRING_ALREADY_GAME] =
        "Já existe um jogo neste grupo.",
        [PCX_TEXT_STRING_WHICH_GAME] =
        "Qual jogo você quer jogar?",
        [PCX_TEXT_STRING_GAME_FULL] =
        "O jogo já está cheio.",
        [PCX_TEXT_STRING_GAME_ALREADY_STARTED] =
        "O jogo já começou.",
        [PCX_TEXT_STRING_NO_GAME] =
        "Nenhum jogo ativo.",
        [PCX_TEXT_STRING_CANCELED] = 
        "Jogo cancelado.",
        [PCX_TEXT_STRING_NAME_FROM_ID] =
        "Mx.%i",
        [PCX_TEXT_STRING_FINAL_CONJUNCTION] =
        " e ",
        [PCX_TEXT_STRING_FINAL_DISJUNCTION] =
        " ou ",
        [PCX_TEXT_STRING_WELCOME] =
        "Bem-vindo. Outros jogadores podem enviar /entrar para entrar no jogo ou você pode "
        "enviar /iniciar para iniciá-lo.\n"
        "\n"
        "Jogo: %s\n"
        "\n"
        "Os jogadores atuais são: \n"
        "%s",
        [PCX_TEXT_STRING_JOIN_BEFORE_START] =
        "Por favor, junte-se ao jogo com /entrar antes de tentar iniciá-lo.",
        [PCX_TEXT_STRING_NEED_MIN_PLAYERS] =
        "Pelo menos %i jogadores são necessários para jogar.",
        [PCX_TEXT_STRING_JOIN_COMMAND] =
        "/entrar",
        [PCX_TEXT_STRING_START_COMMAND] =
        "/iniciar",
        [PCX_TEXT_STRING_CANCEL_COMMAND] = 
        "/cancelar",
        [PCX_TEXT_STRING_HELP_COMMAND] =
        "/ajuda",
        [PCX_TEXT_STRING_RECEIVED_PRIVATE_MESSAGE] =
        "Obrigado pela mensagem. Agora você pode participar de um jogo em um grupo público.",
        [PCX_TEXT_STRING_COUP] =
        "Golpe",
        [PCX_TEXT_STRING_INCOME] =
        "Receita",
        [PCX_TEXT_STRING_FOREIGN_AID] =
        "Ajuda externa",
        [PCX_TEXT_STRING_TAX] =
        "Imposto (Duque)",
        [PCX_TEXT_STRING_ASSASSINATE] =
        "Assassinar (Assassino)",
        [PCX_TEXT_STRING_EXCHANGE] =
        "Trocar (Embaixador)",
        [PCX_TEXT_STRING_STEAL] =
        "Roubar (Capitão)",
        [PCX_TEXT_STRING_ACCEPT] =
        "Aceitar",
        [PCX_TEXT_STRING_CHALLENGE] =
        "Desafio",
        [PCX_TEXT_STRING_BLOCK] =
        "Bloquear",
        [PCX_TEXT_STRING_1_COIN] =
        "1 moeda",
        [PCX_TEXT_STRING_PLURAL_COINS] =
        "%i moedas",
        [PCX_TEXT_STRING_YOUR_CARDS_ARE] =
        "Suas cartas são:",
        [PCX_TEXT_STRING_NOONE] =
        "Ninguém",
        [PCX_TEXT_STRING_WON_1] =
        "%s ganhou!",
        [PCX_TEXT_STRING_WON_PLURAL] =
        "%s ganhou!",
        [PCX_TEXT_STRING_YOUR_GO] =
        "%s, é sua vez. “O que você deseja fazer?”",
        [PCX_TEXT_STRING_WHICH_CARD_TO_LOSE] =
        "Qual carta você quer perder?",
        [PCX_TEXT_STRING_CHALLENGE_SUCCEEDED] =
        "%s desafiou e %s não tinha %s. Então %s perde uma carta.",
        [PCX_TEXT_STRING_CHALLENGE_FAILED] =
        "%s desafiou, mas %s tinha %s. Então %s perdeu uma carta.",
        [PCX_TEXT_STRING_ANNOUNCE_CHALLENGE] =
        "%s não acredita que você tenha %s. \n"
        "Qual carta você quer mostrar para eles?",
        [PCX_TEXT_STRING_NO_CHALLENGE_SO_BLOCK] =
        "Ninguém desafiou. A ação foi bloqueada.",
        [PCX_TEXT_STRING_CLAIM_CARDS_TO_BLOCK] =
        "%s afirma ter %s e bloqueia a ação.",
        [PCX_TEXT_STRING_DOES_SOMEBODY_CHALLENGE] =
        "Alguém quer desafiar?",
        [PCX_TEXT_STRING_OR_BLOCK_NO_TARGET] =
        "Ou alguém quer reivindicar ter %s e bloquear?",
        [PCX_TEXT_STRING_BLOCK_NO_TARGET] =
        "Alguém alega ter %s e quer bloquear?",
        [PCX_TEXT_STRING_OR_BLOCK_WITH_TARGET] =
        "Ou %s, você quer reivindicar ter %s e bloquear?",
        [PCX_TEXT_STRING_BLOCK_WITH_TARGET] =
        "%s, você quer reivindicar ter %s e bloquear?",
        [PCX_TEXT_STRING_WHO_TO_COUP] =
        "%s, quem você quer matar durante o golpe?",
        [PCX_TEXT_STRING_DOING_COUP] =
        "💣 %s faz um golpe contra %s",
        [PCX_TEXT_STRING_DOING_INCOME] =
        "💲 %s leva 1 moeda de renda",
        [PCX_TEXT_STRING_REALLY_DOING_FOREIGN_AID] =
        "Ninguém bloqueado, %s leva as duas moedas",
        [PCX_TEXT_STRING_DOING_FOREIGN_AID] =
        "💴 %s recebe 2 moedas de ajuda externa.",
        [PCX_TEXT_STRING_REALLY_DOING_TAX] =
        "Ninguém desafiou, %s leva as 3 moedas.",
        [PCX_TEXT_STRING_DOING_TAX] =
        "💸 %s alega ter o duque e recebe 3 moedas do imposto",
        [PCX_TEXT_STRING_REALLY_DOING_ASSASSINATION] =
        "Ninguém bloqueou ou desafiou, %s assassina %s",
        [PCX_TEXT_STRING_SELECT_TARGET_ASSASSINATION] =
        "%s, quem você quer assassinar?",
        [PCX_TEXT_STRING_DOING_ASSASSINATION] =
        "🗡 %s quer assassinar %s.",
        [PCX_TEXT_STRING_WHICH_CARDS_TO_KEEP] =
        "Quais cartas você quer manter?",
        [PCX_TEXT_STRING_REALLY_DOING_EXCHANGE] =
        "Ninguém bloqueou ou desafiou, %s troca cartas.",
        [PCX_TEXT_STRING_DOING_EXCHANGE] =
        "🔄 %s afirma ter o embaixador e quer trocar cartas.",
        [PCX_TEXT_STRING_REALLY_DOING_STEAL] =
        "Ninguém bloqueou ou desafiou, %s rouba de %s.",
        [PCX_TEXT_STRING_SELECT_TARGET_STEAL] =
        "%s, de quem você quer roubar?",
        [PCX_TEXT_STRING_DOING_STEAL] =
        "💰 %s quer roubar de %s.",
        [PCX_TEXT_STRING_CHARACTER_NAME_DUKE] =
        "Duque",
        [PCX_TEXT_STRING_CHARACTER_NAME_ASSASSIN] =
        "Assassino",
        [PCX_TEXT_STRING_CHARACTER_NAME_CONTESSA] =
        "Condessa",
        [PCX_TEXT_STRING_CHARACTER_NAME_CAPTAIN] =
        "Capitão",
        [PCX_TEXT_STRING_CHARACTER_NAME_AMBASSADOR] =
        "Embaixador",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_DUKE] =
        "o duque",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_ASSASSIN] =
        "o assassino",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CONTESSA] =
        "a condessa",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_CAPTAIN] =
        "o capitão",
        [PCX_TEXT_STRING_CHARACTER_OBJECT_NAME_AMBASSADOR] =
        "o embaixador",
        [PCX_TEXT_STRING_ROLE_NAME_DRIVER] =
        "Motorista",
        [PCX_TEXT_STRING_ROLE_NAME_LOCKPICK] =
        "Arrombador",
        [PCX_TEXT_STRING_ROLE_NAME_MUSCLE] =
        "Brutamontes",
        [PCX_TEXT_STRING_ROLE_NAME_CON_ARTIST] =
        "Golpista",
        [PCX_TEXT_STRING_ROLE_NAME_LOOKOUT] =
        "Vigia",
        [PCX_TEXT_STRING_ROLE_NAME_SNITCH] =
        "Informante",
        [PCX_TEXT_STRING_ROUND_NUM] =
        "Turno %i / %i",
        [PCX_TEXT_STRING_CHOOSE_HEIST_DIFFICULTY] =
        "%s, você é o líder da equipe. Quantos especialistas você quer no "
        "assalto?",
        [PCX_TEXT_STRING_HEIST_SIZE_CHOSEN] =
        "O assalto precisará dos seguintes %i especialistas:",
        [PCX_TEXT_STRING_DISCUSS_HEIST] =
        "Agora vocês podem discutir entre si sobre qual especialista cada um "
        "irá adicionar ao assalto. Quando estiverem prontos, cada um deve fazer "
        "sua escolha em segredo.",
        [PCX_TEXT_STRING_CARDS_CHOSEN] =
        "Todo mundo fez sua escolha! Os especialistas são:",
        [PCX_TEXT_STRING_NEEDED_CARDS_WERE] =
        "As pessoas necessárias eram:",
        [PCX_TEXT_STRING_YOU_CHOSE] =
        "Você escolhe:",
        [PCX_TEXT_STRING_WHICH_ROLE] =
        "Qual carta você quer escolher?",
        [PCX_TEXT_STRING_HEIST_SUCCESS] =
        "O assalto foi um sucesso! Cada jogador que não escolheu o informante "
        "ganhou %i moedas.",
        [PCX_TEXT_STRING_HEIST_FAILED] =
        "O assalto falhou! Todo mundo que não escolheu o informante perdeu 1 "
        "moeda.",
        [PCX_TEXT_STRING_SNITCH_GAIN_1] =
        "Quem escolheu o informante ganha 1 moeda.",
        [PCX_TEXT_STRING_SNITCH_GAIN_PLURAL] =
        "Quem escolheu o informante ganha %i moedas.",
        [PCX_TEXT_STRING_EVERYONE_SNITCHED] =
        "O assalto falhou e todo mundo foi informante! Ninguém ganha nada.",
        [PCX_TEXT_STRING_NOONE_SNITCHED] =
        "Ninguém escolheu o informante.",
        [PCX_TEXT_STRING_1_SNITCH] =
        "1 informante",
        [PCX_TEXT_STRING_PLURAL_SNITCHES] =
        "%i informante",
        [PCX_TEXT_STRING_GUARD] =
        "Guarda",
        [PCX_TEXT_STRING_GUARD_OBJECT] =
        "o guarda",
        [PCX_TEXT_STRING_GUARD_DESCRIPTION] =
        "Nomeie uma carta diferente do guarda e escolha um jogador. "
        "Se esse jogador tiver essa carta, ele perde a rodada.",
        [PCX_TEXT_STRING_SPY] =
        "Espião",
        [PCX_TEXT_STRING_SPY_OBJECT] =
        "o espião",
        [PCX_TEXT_STRING_SPY_DESCRIPTION] =
        "Olhe para a mão de outro jogador.",
        [PCX_TEXT_STRING_BARON] =
        "Barão",
        [PCX_TEXT_STRING_BARON_OBJECT] =
        "o barão",
        [PCX_TEXT_STRING_BARON_DESCRIPTION] =
        "Secretamente compare as mãos com outro jogador. A pessoa que tiver "
        "a carte de valor mais baixo perde a rodada.",
        [PCX_TEXT_STRING_HANDMAID] =
        "Aia",
        [PCX_TEXT_STRING_HANDMAID_OBJECT] =
        "a aia",
        [PCX_TEXT_STRING_HANDMAID_DESCRIPTION] =
        "Até o seu próximo turno, ignore quaisquer efeitos das cartas de outros jogadores.",
        [PCX_TEXT_STRING_PRINCE] =
        "Príncipe",
        [PCX_TEXT_STRING_PRINCE_OBJECT] =
        "o príncipe",
        [PCX_TEXT_STRING_PRINCE_DESCRIPTION] =
        "Escolha um jogador (pode ser você mesmo) que irá descartar sua mão e "
        "pegue uma nova carta.",
        [PCX_TEXT_STRING_KING] =
        "Rei",
        [PCX_TEXT_STRING_KING_OBJECT] =
        "o rei",
        [PCX_TEXT_STRING_KING_DESCRIPTION] =
        "Escolha outro jogador e troque de mão com ele.",
        [PCX_TEXT_STRING_COMTESSE] =
        "Condessa",
        [PCX_TEXT_STRING_COMTESSE_OBJECT] =
        "a condessa",
        [PCX_TEXT_STRING_COMTESSE_DESCRIPTION] =
        "Se você tem esta carta junto com o rei ou o príncipe em sua mão "
        "você deve descartar esta carta.",
        [PCX_TEXT_STRING_PRINCESS] =
        "Princesa",
        [PCX_TEXT_STRING_PRINCESS_OBJECT] =
        "a princesa",
        [PCX_TEXT_STRING_PRINCESS_DESCRIPTION] =
        "Se você descartar este card, perderá a rodada.",
        [PCX_TEXT_STRING_ONE_COPY] =
        "1 cópia",
        [PCX_TEXT_STRING_PLURAL_COPIES] =
        "%i cópias",
        [PCX_TEXT_STRING_YOUR_CARD_IS] =
        "Sua carta é: ",
        [PCX_TEXT_STRING_VISIBLE_CARDS] =
        "Cartas descartadas: ",
        [PCX_TEXT_STRING_N_CARDS] =
        "Pilha: ",
        [PCX_TEXT_STRING_YOUR_GO_NO_QUESTION] =
        "<b>%p</b>, é sua vez.",
        [PCX_TEXT_STRING_DISCARD_WHICH_CARD] =
        "Qual carta você quer descartar?",
        [PCX_TEXT_STRING_EVERYONE_PROTECTED] =
        "%p descarta %C, mas todos os outros jogadores estão protegidos e isso não tem "
        "efeito.",
        [PCX_TEXT_STRING_WHO_GUESS] =
        "A carta de quem você quer adivinhar?",
        [PCX_TEXT_STRING_GUESS_WHICH_CARD] =
        "Qual carta você quer adivinhar?",
        [PCX_TEXT_STRING_GUARD_SUCCESS] =
        "%p descartou %C e adivinhou corretamente que %p tinha %C. %p perde a "
        "rodada.",
        [PCX_TEXT_STRING_GUARD_FAIL] =
        "%p descartou %C e errou ao achar que %p tinha %C.",
        [PCX_TEXT_STRING_WHO_SEE_CARD] =
        "A carta de quem você quer ver?",
        [PCX_TEXT_STRING_SHOWS_CARD] =
        "%p descartou %C e fez %p secretamente mostrar sua carta.",
        [PCX_TEXT_STRING_TELL_SPIED_CARD] =
        "%p tem %C",
        [PCX_TEXT_STRING_WHO_COMPARE] =
        "Com quem você quer comparar cartas?",
        [PCX_TEXT_STRING_COMPARE_CARDS_EQUAL] =
        "%p descartou %C e comparou sua mão com %p. As cartas foram iguais "
        "e ninguém perde a rodada.",
        [PCX_TEXT_STRING_COMPARE_LOSER] =
        "%p descartou %C e comparou sua mão com %p. %p teve a menor "
        "carta e perde o turno.",
        [PCX_TEXT_STRING_TELL_COMPARE] =
        "Você tem %C e %p tem %C",
        [PCX_TEXT_STRING_DISCARDS_HANDMAID] =
        "%p descartou %C e ficará protegido até o próximo turno",
        [PCX_TEXT_STRING_WHO_PRINCE] =
        "Quem você quer fazer descartar a mão?",
        [PCX_TEXT_STRING_PRINCE_SELF] =
        "%p descartou %C e o fez descartar %C",
        [PCX_TEXT_STRING_PRINCE_OTHER] =
        "%p descartou %C e fez %p descartar %C",
        [PCX_TEXT_STRING_FORCE_DISCARD_PRINCESS] =
        " e assim perder a rodada.",
        [PCX_TEXT_STRING_FORCE_DISCARD_OTHER] =
        " e pegar uma nova carta.",
        [PCX_TEXT_STRING_WHO_EXCHANGE] =
        "Com quem você quer trocar a mão?",
        [PCX_TEXT_STRING_TELL_EXCHANGE] =
        "Você deu %C para %p e recebeu %C",
        [PCX_TEXT_STRING_EXCHANGES] =
        "%p descartou o rei e trocou de mão com %p.",
        [PCX_TEXT_STRING_DISCARDS_COMTESSE] =
        "%p descartou %C",
        [PCX_TEXT_STRING_DISCARDS_PRINCESS] =
        "%p descartou %C e perdeu a rodada.",
        [PCX_TEXT_STRING_EVERYBODY_SHOWS_CARD] =
        "A rodada terminou e todo mundo mostra sua carta:",
        [PCX_TEXT_STRING_SET_ASIDE_CARD] =
        "A carta oculta era %c",
        [PCX_TEXT_STRING_WINS_ROUND] =
        "💘 %p vence a rodada e ganha um ponto de afeto da princesa.",
        [PCX_TEXT_STRING_WINS_PRINCESS] =
        "🏆 %p tem %i pontos de afeto e ganha o jogo!",
};

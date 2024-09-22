/*
 * Pucxobot - A bot and website to play some card games
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

#include "pcx-snitch-help.h"

#include "pcx-text.h"

const char *
pcx_snitch_help[] = {
        [PCX_TEXT_LANGUAGE_ESPERANTO] =
        "<b>RESUMO DE LA REGULOJ:</b>\n"
        "\n"
        "Perfidulo estas kartludo de duonkunlaboro kaj blufado.\n"
        "\n"
        "Vi ĉiuj estas rabistoj kiuj kunlaboras por organizi rabadojn de "
        "bankoj. Estos 8 raŭndoj kaj en ĉiu raŭndo estos unu rabestro.\n"
        "\n"
        "<b>La kontrakto</b>\n"
        "\n"
        "La rabestro elektas kiom da roluloj oni bezonos por la rabado, kaj "
        "tiel elektas la mafacilecon. Tiam oni prenas kartojn el la kartaro "
        "kiuj indikas kiu estas ĉiu el tiuj roluloj. Tiuj kartoj formas la "
        "kontrakton kaj por sukcesi la rabadon oni devas dungi ĉiujn tiujn "
        "rolulojn.\n"
        "\n"
        "<b>La diskuto kaj rabado</b>\n"
        "\n"
        "Nun estas la diskuta fazo. Ĉiu havas en sia mano 10 kartojn. 7 el ili "
        "reprezentas rolulojn. Vi devas kunlabori por provi havi la bezonatajn "
        "rolulojn. Fine ĉiu sekrete elektas sian karton kaj kiam ĉiu estos "
        "preta la kartoj estos malkaŝitaj samtempe. Se vi sukcesas montri la "
        "bezonatajn rolulojn ĉiu gajnas la saman kvanton da moneroj kiel la "
        "grandeco de la kontrakto.\n"
        "\n"
        "<b>La perfiduloj</b>\n"
        "\n"
        "Tamen en ĉies mano estas ankaŭ 3 perfiduloj. Se la rabado malsukcesas "
        "kaj iuj elektis la perfidulon, anstataŭ ŝteli monon de la banko la "
        "perfiduloj prenas monon de la aliaj ludantoj! Ĉiu kiu ne elektis "
        "perfidulon perdas 1 moneron kaj la kolekto de mono estas dividita "
        "inter la perfiduloj. Se la sumo ne estas egale dividebla, oni prenas "
        "pliajn monerojn el la banko por egaligi ĝin.\n"
        "\n"
        "Post 8 raŭndoj, tiu kiu havas la plej multon da mono gajnas la "
        "partion.\n",
        [PCX_TEXT_LANGUAGE_FRENCH] =
        "<b>RÉSUMÉ DES RÈGLES :</b>\n"
        "\n"
        "Balance est un jeu de cartes de mi-collaboration et de bluff.\n"
        "\n"
        "Vous êtes tous des cambrioleurs qui travaillent ensembles pour "
        "organiser un braquage. Il y a 8 tours et pour chaque tour il y a "
        "un chef de braquage.\n"
        "\n"
        "<b>Le contrat</b>\n"
        "\n"
        "Le chef de braquage choisit combien de personnages il faudra pour "
        "le braquage, et ainsi il choisit la difficulté. Puis il pioche "
        "des cartes qui indiquent lesquels sont ces personnages. Ces "
        "cartes-là forment le contrat et pour réussir le braquage il faut "
        "embaucher tous les personnages indiqués.\n"
        "\n"
        "<b>La discussion et le braquage</b>\n"
        "\n"
        "Maintenant c’est la phase de discussion. Tout le monde a dans sa "
        "main 10 cartes. 7 d’entre eux représentent des personnages. Il "
        "faut collaborer pour essayer d’avoir les cartes nécessaires. "
        "Finalement, chacun choisit sa carte en secret et quand tout le "
        "monde sera prêt les cartes seront révélées au même temps. Si vous "
        "réussissez à montrer toutes les cartes du contrat, tout le monde "
        "gagne la même quantité de pièces d’or que la taille du contrat.\n"
        "\n"
        "<b>Les balances</b>\n"
        "\n"
        "Pourtant, dans la main de chacun, il y a aussi 3 balances. Si le "
        "braquage est un échec et au moins une personne choisit la "
        "balance, à la place de voler de l’argent de la banque les "
        "balances prennent de l’argent des autres joueurs ! Tous ce qui "
        "n’ont pas choisi la balance perdent une pièce d’or et la "
        "collection de pièces est partagée entre les balances. Si la somme "
        "d’or n’est pas divisible également, des pièces de la banque sont "
        "ajoutées pour le faire égale.\n"
        "\n"
        "Après les huit tours, ce qui a le plus d’or remporte la partie.",
        [PCX_TEXT_LANGUAGE_ENGLISH] =
        "<b>SUMMARY OF THE RULES:</b>\n"
        "\n"
        "Snitch is a card game of semi-collaboration and bluff.\n"
        "\n"
        "You are all criminals who are working together to perform a heist. "
        "There are 8 rounds and in each round there is one gang leader.\n"
        "\n"
        "<b>The contract</b>\n"
        "\n"
        "The gang leader chooses how many specialists the heist will have and "
        "by doing so also chooses the difficulty level. They then take that "
        "many cards from the deck and show them to everybody. These cards "
        "form the contract for the heist. For it to be a success each one of "
        "these specialists must take part.\n"
        "\n"
        "<b>The discussion and the heist</b>\n"
        "\n"
        "Now it’s time for the discussion phase. Everybody has 10 cards in "
        "their hand. 7 of them represent specialists. You need to work "
        "together to try to get the cards needed for the heist. Finally when "
        "everyone has chosen their card they are all revealed at the same "
        "time. If you manage to show all the cards needed for the contract, "
        "everybody wins the same number of coins as the size of the contract.\n"
        "\n"
        "<b>The snitches</b>\n"
        "\n"
        "However, in everybody’s hand there are also 3 snitches. If the heist "
        "fails and at least one person chose the snitch card, instead of "
        "taking money from the bank the snitches steal money from the other "
        "players! Everybody who didn’t snitch loses one coin and the loot is "
        "divided between all of the snitches. If the amount can’t be divided "
        "evenly it is rounded up by taking extra coins from the bank.\n"
        "\n"
        "After the 8 rounds, the player with the most coins wins the game.",
        [PCX_TEXT_LANGUAGE_PT_BR] =
        "<b>RESUMO DAS REGRAS:</b>\n"
        "\n"
        "Informante é um jogo de cartas de semi-colaboração e blefe.\n"
        "\n"
        "Vocês são todos criminosos que estão trabalhando juntos para realizar um assalto. "
        "Há 8 rodadas e em cada rodada há um líder da equipe.\n"
        "\n"
        "<b>O contrato</b>\n"
        "\n"
        "O líder da equipe escolhe quantos especialistas quer para o assalto e "
        "ao fazer isso, também escolhe o nível de dificuldade. Quanto maior o "
        "número de especialistas, maior a dificuldade. O jogo mostra o tipo e "
        "quantidade necessária para o assalto. Para que seja um sucesso, todos os "
        "especialistas devem participar da equipe.\n"
        "\n"
        "<b>A discussão e o assalto</b>\n"
        "\n"
        "Agora é hora da fase de discussão. Todo mundo começa com 10 opções em "
        "sua mão. 7 delas são especialistas. Vocês precisarão trabalhar "
        "em conjunto para tentar obter a equipe necessárias para o assalto. Quando "
        "todo mundo escolher quem irá participar, as escolhas são reveladas ao mesmo "
        "tempo. Se conseguirem os especialistas necessários para o assalto, "
        "todos ganham o mesmo número de moedas conforme o contrato.\n"
        "\n"
        "<b>Os informantes</b>\n"
        "\n"
        "No entanto, na mão de todos, há também 3 informantes. Se o assalto "
        "falha e pelo menos uma pessoa escolheu o informante, em vez de "
        "tirarem dinheiro do banco, os informantes roubam dinheiro dos outros "
        "jogadores! Todo mundo que não escolheu o informante perde uma moeda e o total é "
        "dividido entre todos os informantes. Se a quantia não puder ser dividida "
        "uniformemente, será arredondada e as moedas extras irão para o banco.\n"
        "\n"
        "Após as 8 rodadas, o jogador com mais moedas ganha o jogo."
};

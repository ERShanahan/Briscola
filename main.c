#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define n_players 5
#define n_hand 8
#define n_cards 40
#define minimax_depth 4

typedef struct card{
    int value;
    int suit;
} card;

#define relative_call_limit 10000
const int relative_value[10] = {1, 2, 3, 5, 7, 10, 15, 20, 75, 150};
const int true_value[10] = {0, 0, 0, 0, 0, 2, 3, 4, 10, 11};

size_t getline(char **lineptr, size_t *n, FILE *stream) {
    char *bufptr = NULL;
    char *p = bufptr;
    size_t size;
    int c;

    if (lineptr == NULL) {
        return -1;
    }
    if (stream == NULL) {
        return -1;
    }
    if (n == NULL) {
        return -1;
    }
    bufptr = *lineptr;
    size = *n;

    c = fgetc(stream);
    if (c == EOF) {
        return -1;
    }
    if (bufptr == NULL) {
        bufptr = malloc(128);
        if (bufptr == NULL) {
            return -1;
        }
        size = 128;
    }
    p = bufptr;
    while(c != EOF) {
        if ((p - bufptr) > (size - 1)) {
            size = size + 128;
            bufptr = realloc(bufptr, size);
            if (bufptr == NULL) {
                return -1;
            }
        }
        *p++ = c;
        if (c == '\n') {
            break;
        }
        c = fgetc(stream);
    }

    *p++ = '\0';
    *lineptr = bufptr;
    *n = size;

    return p - bufptr - 1;
}

typedef struct player{
    int bot;
    int calling;
    int position;
    int team;
    card hand[n_hand];
} player;

typedef struct game_state{
    player p;
    int partner_prob[n_players];
    card cards_taken[n_players][n_cards];
    card cards_remaining[n_cards];
    card cards_played[n_cards];
    card cards_tabled[n_players];
    int starting;
    int turn;
    int bris;
    int num_cards_played;
} game_state;

/* Prints specs of param position */
void print_state(game_state position){

    printf("\n");
    printf("Pov Position: %d\n", position.p.position);
    printf("Starting index: %d\n", position.starting);
    printf("Turn: %d\n", position.turn);
    printf("Bris: %d\n", position.bris);

    printf("Pov hand: ");
    for(int i = 0; i < n_hand; i++){
        if(position.p.hand[i].value != -1){
            printf("%d%d, ", position.p.hand[i].value, position.p.hand[i].suit);
        }
    }
    printf("\n");

    printf("Num Cards Played: %d\n", position.num_cards_played);
    printf("Cards Played: ");
    for(int i = 0; i < position.num_cards_played; i++){
        printf("%d%d, ", position.cards_played[i].value, position.cards_played[i].suit);
    }
    printf("\n");

    printf("Cards Tabled:\n");
    for(int i = 0; i < n_players; i++){
        if(position.cards_tabled[i].value != -1){
            printf("%d%d, ", position.cards_tabled[i].value, position.cards_tabled[i].suit);
        }
    }
    printf("\n");

    printf("Cards Remaining: ");
    for(int i = 0; i < n_cards; i++){
        if(position.cards_remaining[i].value != -1){
            printf("%d%d, ", position.cards_remaining[i].value, position.cards_remaining[i].suit);
        }
    }
    printf("\n");

    printf("Probability Array: ");
    for(int i = 0; i < n_players; i++){
        printf("%d, ", position.partner_prob[i]);
    }
    printf("\n");

    printf("Cards taken:\n");
    for(int i = 0; i < n_players; i++){
        printf("Player %d: ", i);
        for(int j = 0; j < n_cards; j++){
            if(position.cards_taken[i][j].value != -1){
                printf("%d%d ", position.cards_taken[i][j].value, position.cards_taken[i][j].suit);
            }   
        }
        printf("\n");
    }
    printf("\n");
    printf("\n");
}

/* Prints the hand of the given player in form ex. 00 01 02 03 01 11 12 13*/
void print_hand(player p){
    for(int i = 0; i < n_hand; i++){
        if(p.hand[i].value != -1){
            printf("%d%d ", p.hand[i].value, p.hand[i].suit);
        }
    }
    printf("\n");
}

/* In-place randomization of card_arr, n = sizeof card_arr */
int shuffle_card_arr(card* card_arr, size_t n) {
    // Seed the random number generator
    srand(time(NULL));

    for (size_t i = n - 1; i > 0; i--) {
        // Generate a random index j such that 0 <= j <= i
        size_t j = rand() % (i + 1);

        // Swap card_arr[i] and card_arr[j]
        card temp = card_arr[i];
        card_arr[i] = card_arr[j];
        card_arr[j] = temp;
    }

    return 0; // Indicate success
}

/* Initializes player array with n players, first n_hand cards goes to p1, so on*/
void init_players(player* p, int n, card* card_arr){
    for(int i = 0; i < n; i++){
        p[i].bot = 1;
        p[i].team = -1;
        p[i].calling = 1;
        p[i].position = i;
        for(int j = 0; j < n_hand; j++){
            p[i].hand[j] = card_arr[j + (i*n_hand)];
        }
    }
}

/* Creates card from integer representation of card */
void make_card(card* c, int n){
    c->value = n/10;
    c->suit = n%10;
}

/* Deep copies content of a to b */
void cp_card(card a, card* b){
    b->value = a.value;
    b->suit = a.suit;
}

/* Deep copies content of set to dest for n cards */
void cp_set(card* set, card* dest, int n){
    for(int i = 0; i < n; i++){
        dest[i].suit = set[i].suit;
        dest[i].value = set[i].value;
    }
}

/* Deep copies game_state state to dest */
void cp_state(game_state state, game_state* dest){
    dest->p.team = state.p.team;
    dest->p.position = state.p.position;
    cp_set(state.p.hand, dest->p.hand, n_hand);
    for(int i = 0; i < n_players; i++){
        dest->partner_prob[i] = state.partner_prob[i];
        cp_set(state.cards_taken[i], dest->cards_taken[i], n_cards);
    }
    cp_set(state.cards_played, dest->cards_played, state.num_cards_played);
    cp_set(state.cards_remaining, dest->cards_remaining, n_cards);
    cp_set(state.cards_tabled, dest->cards_tabled, n_players);
    dest->starting = state.starting;
    dest->bris = state.bris;
    dest->num_cards_played = state.num_cards_played;
    dest->turn = state.turn;
}

/* Makes card c a null card {-1, -1}*/
void set_null(card* c){
    c->value = -1;
    c->suit = -1;
}

/* Returns the total score contained in a set of n cards */
int score(card* set, int n){
    int score = 0;
    for(int i = 0; i < n; i++){
        if(set[i].value != -1){
            score += true_value[set[i].value];
        }
    }
    return score;
}

/* Returns the index in set of size n where card first appears, or -1 if not contained in set */
int contains(card* set, int n, card card){
    for(int i = 0; i < n; i++){
        if(set[i].value == card.value && set[i].suit == card.suit)
            return i;
    }
    return -1;
}

/* Returns the index in the set of size n where the suit first appears, or -1 if not contained in set */
int contains_suit(card* set, int n, int suit){
    for(int i = 0; i < n; i++){
        if(set[i].suit == suit){
            return i;
        }
    }
    return -1;
}

/* Returns the index of the highest card in the set of size n that is of the suit given */
int highest_card(card* set, int n, int suit){
    int max = -1;
    int index = -1;
    for(int i = 0; i < n; i++){
        if(set[i].suit == suit){
            if(set[i].value > max){
                max = set[i].value;
                index = i;
            }
        }
    }
    return index;
}

/* Returns the number of a suit in a set of size n */
int num_of_suit(card* set, int n, int suit){
    int num = 0;
    for(int i = 0; i < n; i++){
        if(set[i].suit == suit){
            num++;
        }
    }
    return num;
}

/* Alters the position such that the cards_tabled are accurately distributed to the player who played highest */
int collect_table(game_state* position){
    int index_highest = highest_card(position->cards_tabled, n_players, position->bris);
    if(index_highest == -1){
        index_highest = highest_card(position->cards_tabled, n_players, position->cards_tabled[0].suit);
    }
    int offset = 0;
    int player_reward = (index_highest+position->starting)%5;
    for(int i = 0; i < n_cards; i++){
        if(position->cards_taken[player_reward][i].value == -1){
            offset = i;
            break;
        }
    }
    for(int i = 0; i < n_players; i++){
        cp_card(position->cards_tabled[i], &position->cards_taken[player_reward][i+offset]);
    }
    return index_highest;
}

/* Initializes all cards in set of size n to the null card {-1, -1} */
int init_set_null(card* set, int n){
    for(int i = 0; i < n; i++){
        set_null(&set[i]);
    }
}

/* Determines player p's strongest calling_suit considering the last_called value, -1 if not strong enough to call */
int calling_suit(player p, int last_called){
    
    int bonus = score(p.hand, n_hand) * 50;
    int spades[n_hand];
    int clubs[n_hand];
    int hearts[n_hand];
    int diamonds[n_hand];
    int n_spades = 0, n_clubs = 0, n_hearts = 0, n_diamonds = 0;
    int cv_spades = 0, cv_clubs = 0, cv_hearts = 0, cv_diamonds = 0;

    for(int i = 0; i < n_hand; i++){
        if(p.hand[i].suit == 0){
            spades[n_spades] = p.hand[i].value;
            n_spades++;
        }
        if(p.hand[i].suit == 1){
            clubs[n_clubs] = p.hand[i].value;
            n_clubs++;
        }
        if(p.hand[i].suit == 2){
            hearts[n_hearts] = p.hand[i].value;
            n_hearts++;
        }
        if(p.hand[i].suit == 3){
            diamonds[n_diamonds] = p.hand[i].value;
            n_diamonds++;
        }
    }

    for(int i = 0; i < n_spades; i++){
        cv_spades += relative_value[spades[i]];
    }
    for(int i = 0; i < n_clubs; i++){
        cv_clubs += relative_value[clubs[i]];
    }
    for(int i = 0; i < n_hearts; i++){
        cv_hearts += relative_value[hearts[i]];
    }
    for(int i = 0; i < n_diamonds; i++){
        cv_diamonds += relative_value[diamonds[i]];
    }

    int cv_max;
    if(last_called == -1){
        cv_max = MAX(cv_spades, MAX(cv_clubs, MAX(cv_hearts, cv_diamonds)));

        if(cv_max == cv_spades){
            return 0;
        }
        if(cv_max == cv_clubs){
            return 1;
        }
        if(cv_max == cv_hearts){
            return 2;
        }
        if(cv_max == cv_diamonds){
            return 3;
        }
        return -1;
    }

    int rv_last_called = relative_value[last_called];
    int rcv_spades, rcv_clubs, rcv_hearts, rcv_diamonds = 100;
    
    rcv_spades = cv_spades * n_spades * rv_last_called + bonus;
    rcv_clubs = cv_clubs * n_clubs * rv_last_called + bonus;
    rcv_hearts = cv_hearts * n_hearts * rv_last_called + bonus;
    rcv_diamonds = cv_diamonds * n_diamonds * rv_last_called + bonus;

    int rcv_max = MAX(rcv_spades, MAX(rcv_clubs, MAX(rcv_hearts, rcv_diamonds)));
    if(rcv_max > relative_call_limit){
        if(rcv_max == rcv_spades){
            return 0;
        }
        if(rcv_max == rcv_clubs){
            return 1;
        }
        if(rcv_max == rcv_hearts){
            return 2;
        }
        if(rcv_max == rcv_diamonds){
            return 3;
        }
    }
    return -1;
}

/* Determines player p's call based on strongest suit and last_called value */
int call(player p, int last_called){

    int suit = calling_suit(p, last_called);
    if(suit == -1){
        return -1;
    }
    
    int calling_card;
    if(last_called == -1){
        calling_card = 9;
    }else{
        calling_card = last_called - 1;
    }

    int in_hand = 1;
    while(in_hand){
        if(calling_card == -1){
            return -1;
        }
        in_hand = 0;
        for(int i = 0; i < n_hand; i++){
            if(p.hand[i].suit == suit && p.hand[i].value == calling_card){
                calling_card--;
                in_hand = 1;
            }
        }
    }
    return calling_card;
}

/* Sets the state of position given the initial card_arr, player array players, start index, and final call */
int setup_state(game_state* position, card* card_arr, player* players, int start, card final_call){
    position->p.position = players[start].position;
    position->p.team = players[start].team;
    position->p.bot = players[start].bot;
    cp_set(players[start].hand, position->p.hand, n_hand);
    for(int i = 0; i < n_players; i++){
        position->partner_prob[i] = players[i].team;
        init_set_null(position->cards_taken[i], n_cards);
    }
    position->bris = final_call.suit;
    init_set_null(position->cards_tabled, n_players);
    init_set_null(position->cards_played, n_cards);
    cp_set(card_arr, position->cards_remaining, n_cards);
    
    position->num_cards_played = 0;
    position->starting = start;
    position->turn = 0;
}

/* Transfers the pov of the game_state to the next player, deep copies current pov back to player array and next player into pov */
int next_state(game_state* position, player* players, int new_starting){

    cp_set(position->p.hand, players[position->p.position].hand, n_hand);
    int new_player = new_starting;
    position->p.position = players[new_player].position;
    position->p.team = players[new_player].team;
    position->p.bot = players[new_player].bot;
    cp_set(players[new_player].hand, position->p.hand, n_hand);

}

/* Plays card c into the position given, deep copies card into both cards_played and cards_tabled, increments num_cards_played and turn */
int play_card(game_state* position, card c){
    cp_card(c, &position->cards_played[position->num_cards_played]);
    cp_card(c, &position->cards_tabled[position->turn % n_players]);
    position->num_cards_played++;
    position->turn++;
}

/* Determines if game is over by num_cards_played */
int game_over(game_state position){
    if(position.num_cards_played >= 40) return 1;
    return 0;
}

/* Evaluates position by the total sum of taken cards multiplied by the team of the player who took them */
/* eval > 0 means that the caller/partner is winning, eval < 0 means that the corp is winning */
int evaluation(game_state position){
    int sum = 0;
    if(position.num_cards_played >= 40){
        for(int i = 0; i < n_players; i++){
            sum += position.partner_prob[i] * score(position.cards_taken[i], n_cards);
        }
        return sum;
    }else{
        float played = position.num_cards_played;
        int weight = n_players - played/((n_cards/4)*3);
        for(int i = 0; i < n_players; i++){
            if(i == position.p.position){
                sum += position.partner_prob[i] * score(position.cards_taken[i], n_cards)
                        + (score(position.p.hand, n_cards) * weight)
                        + (position.p.team * score(position.cards_remaining, n_cards) * num_of_suit(position.p.hand, n_hand, position.bris));
            }else{
                sum += position.partner_prob[i] * score(position.cards_taken[i], n_cards)
                        + (score(position.cards_remaining, n_cards) * weight)
                        + (position.p.team * score(position.cards_remaining, n_cards) * num_of_suit(position.cards_remaining, n_cards, position.bris));
            }
        }
        return sum;
    }
}

/* Minimax algorithm to maximize or minimize (from param max) the evaluation at the depth given */
int minimax(game_state* position, int max, int depth){

    if(depth <= 0 || game_over(*position)){
        collect_table(position);
        return evaluation(*position);
    }

    int eval = max * -1000000;
    if(position->turn < 5){
        if(position->p.position == (position->starting+position->turn)%n_players){
            for(int i = 0; i < n_hand; i++){
                if(position->p.hand[i].value != -1){
                    game_state new_position;
                    cp_state(*position, &new_position);
                    play_card(&new_position, position->p.hand[i]);
                    set_null(&new_position.p.hand[i]);
                    if(max > 0){
                        eval = MAX(minimax(&new_position, position->partner_prob[(position->starting+position->turn)%n_players], depth), eval);
                    }else{
                        eval = MIN(minimax(&new_position, position->partner_prob[(position->starting+position->turn)%n_players], depth), eval);
                    }
                }
            }
        }else{
            for(int i = 0; i < n_cards; i++){
                if(position->cards_remaining[i].value != -1){
                    if(contains(position->p.hand, n_hand, position->cards_remaining[i]) == -1){
                        game_state new_position;
                        cp_state(*position, &new_position);
                        play_card(&new_position, position->cards_remaining[i]);
                        set_null(&new_position.cards_remaining[i]);
                        if(max > 0){
                            eval = MAX(minimax(&new_position, position->partner_prob[(position->starting+position->turn)%n_players], depth-1), eval);
                        }else{
                            eval = MIN(minimax(&new_position, position->partner_prob[(position->starting+position->turn)%n_players], depth-1), eval);
                        }
                    }
                }
            }
        }
        return eval;
    }
    int index_highest = collect_table(position);
    position->starting = (index_highest + position->starting) % n_players;
    position->turn = 0;
    return minimax(position, position->partner_prob[position->starting], depth);
}

/* Given an evaluation array, returns the index of the best evaluation given if player is maximizing or minimizing */
int index_to_play(int* eval_arr, player p){
    int index = 0;
    int eval = p.team * -1000000;
    for(int i = 0; i < n_hand; i++){
        if(p.hand[i].value != -1){
            if(p.team > 0){
                if(eval_arr[i] > eval){
                    eval = eval_arr[i];
                    index = i;
                }
            }else{
                if(eval_arr[i] < eval){
                    eval = eval_arr[i];
                    index = i;
                }
            }
        }
    }
    return index;
}

/* Given a game state, provides the maximal gain for pov player by evaluating each card in hand, returns the best evaluated card's index in hand */
int make_decision(game_state game){
    int eval_arr[n_hand];
    for(int i = 0; i < n_hand; i++){
        eval_arr[i] = game.p.team*-1000;
    }

    printf("Player %d hand: ", game.p.position);
    print_hand(game.p);

    for(int i = 0; i < n_hand; i++){
        if(game.p.hand[i].value != -1){
            game_state new_position;
            cp_state(game, &new_position);
            play_card(&new_position, game.p.hand[i]);
            set_null(&new_position.p.hand[i]);
            printf("trying card %d...", i);
            eval_arr[i] = minimax(&new_position, game.p.team, minimax_depth);
            printf("eval of %d\n", eval_arr[i]);
        }
    }
    return index_to_play(eval_arr, game.p);
}

/* Given predefined arrays of cards and players, play game among 5 bots */
int simulate(card* card_arr, player* players){
    
    int callers = n_players;
    int index = 0;
    int caller = -1;
    int calling_card = -1;
    while(callers > 1){
        int temp;
        if(players[index%n_players].calling > 0){
            temp = call(players[index%n_players], calling_card);
            if(temp == -1){
                players[index%n_players].calling = 0;
                callers--;
                index++;
                continue;
            }
        }else{
            index++;
            continue;
        }
        caller = index%n_players;
        calling_card = temp;
        index++;
    }

    card final_call = {calling_card, calling_suit(players[caller], -1)};
    printf("Final Caller: Player %d calls %d of %d\n\n", caller, final_call.value, final_call.suit);

    players[caller].team = 1;
    for(int i = 0; i < n_players; i++){
        if(contains(players[i].hand, n_hand, final_call) >= 0){
            players[i].team = 1;
            break;
        }
    }

    for(int i = 0; i < n_players; i++){
        printf("Player %d team is %d\n", i, players[i].team);
    }
    printf("\n");

    game_state game;
    setup_state(&game, card_arr, players, caller, final_call);
    
    int count = 0;
    int play;
    while(game.num_cards_played < 40){
        if(count%n_players == 0 && game.num_cards_played > 0){
            game.starting = (collect_table(&game) + game.starting) % n_players;
            next_state(&game, players, game.starting);
            print_state(game);
            printf("Current Evaluation: %d\n", evaluation(game));
        }
        if(game.num_cards_played == 0){
            print_state(game);
        }
        play = make_decision(game);
        printf("Player %d plays %d%d\n", game.p.position, game.p.hand[play].value, game.p.hand[play].suit);
        play_card(&game, game.p.hand[play]);
        set_null(&game.p.hand[play]);
        set_null(&game.cards_remaining[play+(game.p.position*n_hand)]);
        next_state(&game, players, (game.p.position+1) % n_players);
        count++;
    }

    collect_table(&game);
    print_state(game);
    printf("Current Evaluation: %d\n", evaluation(game));

    return evaluation(game);
}

/* Given predefined arrays of cards and players, play game with predetermined number of bots */
int run_game(card* card_arr, player* players){

    printf("Begin Calling Phase:\n"
            "\tPlease enter a valid call (-1 to pass).\n");

    int callers = n_players;
    int index = 0;
    int caller = -1;
    int calling_card = -1;
    while(callers > 1){
        int temp;
        if(players[index%n_players].calling > 0){
            print_hand(players[index%n_players]);
            printf("Player %d: ", index%n_players);
            if(players[index%n_players].bot){
                temp = call(players[index%n_players], calling_card);
                printf("%d\n", temp);
                if(temp == -1){
                    players[index%n_players].calling = 0;
                    callers--;
                    index++;
                    continue;
                }
            }else{
                scanf("%d", &temp);
                if(temp == -1){
                    players[index%n_players].calling = 0;
                    callers--;
                    index++;
                    continue;
                }
                if(!(temp < 9 && temp >= 0) || (temp > calling_card && calling_card != -1)){
                    printf("Error: Invalid Call %d\n", temp);
                    continue;
                }
            }
        }else{
            index++;
            continue;
        }
        caller = index%n_players;
        calling_card = temp;
        index++;
    }

    card final_call;
    if(players[caller].bot){
        final_call.value = calling_card;
        final_call.suit = calling_suit(players[caller], -1);
    }else{
        int suit;
        printf("Suit? ");
        scanf("%d", &suit);
        final_call.value = calling_card;
        final_call.suit = suit;
    }
    printf("Final Caller: Player %d calls %d of %d\n\n", caller, final_call.value, final_call.suit);

    players[caller].team = 1;
    for(int i = 0; i < n_players; i++){
        if(contains(players[i].hand, n_hand, final_call) >= 0){
            players[i].team = 1;
            break;
        }
    }

    for(int i = 0; i < n_players; i++){
        printf("Player %d team is %d\n", i, players[i].team);
    }
    printf("\n");

    game_state game;
    setup_state(&game, card_arr, players, caller, final_call);

    int count = 0;
    int play;
    int play_index;
    while(game.num_cards_played < 40){
        if(count%n_players == 0 && game.num_cards_played > 0){
            game.starting = (collect_table(&game) + game.starting) % n_players;
            next_state(&game, players, game.starting);
            print_state(game);
            printf("Current Evaluation: %d\n", evaluation(game));
        }

        if(game.p.bot){
            play = make_decision(game);
        }else{
            print_hand(game.p);
            printf("Player %d? ", game.p.position);
            scanf("%d", &play);
            card c;
            make_card(&c, play);
            if((play_index = contains(game.p.hand, n_hand, c)) == -1){
                printf("Error: Please play card from hand.");
                continue;
            }
            play = play_index;
        }
        
        printf("Player %d plays %d%d\n", game.p.position, game.p.hand[play].value, game.p.hand[play].suit);
        play_card(&game, game.p.hand[play]);
        set_null(&game.p.hand[play]);
        set_null(&game.cards_remaining[play+(game.p.position*n_hand)]);
        next_state(&game, players, (game.p.position+1) % n_players);
        count++;
    }

    collect_table(&game);
    print_state(game);
    printf("Current Evaluation: %d\n", evaluation(game));

    return evaluation(game);
}

/* When the user uses -m flag, allow the user to set the hands of each player (manual deal) */
int set_card_arr(card* card_arr){

    printf("Please assign 8 cards to each player (00 represents 2 of spades)\n"
            "Ex. \"03 44 42 81 91 72 01 00\" is a valid hand\n"
            "Please input all eight cards per line.\n");

    card buffer[n_cards];
    init_set_null(buffer, n_cards);

    int cards = 0;
    int player = 0;
    char* delim = " ";
    char* scan_buffer = NULL;
    int hand_arr[n_hand];
    size_t size = 0;
    size_t len = 0;
    
    loop:
    while(cards < n_cards){

        printf("Assign cards to player %d:\n", player);
        if( (len = getline(&scan_buffer, &size, stdin)) != -1){
            int n = 0;
            char* buf = strtok(scan_buffer, delim);
            while(buf != NULL && n < n_hand){
                hand_arr[n] = atoi(buf);
                buf = strtok(NULL, delim);
                n++;
            }
            for(int i = 0; i < n_hand; i++){
                card c;
                make_card(&c, hand_arr[i]);
                if(contains(card_arr, n_cards, c) == -1 || contains(buffer, n_cards, c) >= 0){
                    printf("Error: Invalid Card Entry %d%d\n", c.value, c.suit);
                    goto loop;
                }
            }
            for(int i = 0; i < n_hand; i++){
                card c;
                c.value = hand_arr[i]/10;
                c.suit = hand_arr[i]%10;
                cp_card(c, &buffer[i+cards]);
            }
        }else{
            perror("getline");
            exit(EXIT_FAILURE);
        }

        cards+=8;
        player++;
    }
    cp_set(buffer, card_arr, n_cards);
    free(scan_buffer);
}

/* Display usage of program to user */
void help_msg(char* program){
    printf("Usage: %s [-s integer] [-p position type] [-m] [-h]\n\n"
    "  -s number of simulations\tDefault to \"1\";\n"
    "  -p position \t\tDefault to \"0\" (position 0-4);\n"
    "  -m manual deal\t\tDefault to automatic deal;\n"
    "  -h\t\t\t\tDisplay this help info.\n", program);
    printf("\nNOTE: Program defaults to simulate a single game of bots.\n");
}

/* Function to print error, help msg, then exit */
void exit_help(char* error_flag, char* program){
    fprintf(stderr, "Error: %s.\n", error_flag);
    help_msg(program);
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv){

    //Setup of flags
    int simulation = 1;
    int simulation_count = 1;
    int pvb = 0;
    int bot_pos = 0;
    int manual_deal = 0;

    //Get options from command
    int option;
    int argc_count = 1;
    const char* options = ":spmh";
    while( (option = getopt(argc, argv, options)) != -1 ){
        switch(option){  
            case 's':
                if(optind < argc){
                    simulation_count = atoi(argv[optind]);
                    argc_count+=2;
                }else exit_help("s flag", argv[0]);
                break;
            case 'p':
                if(optind < argc){
                    simulation = 0;
                    pvb = 1;
                    bot_pos = atoi(argv[optind]);
                    argc_count+=2;
                }else exit_help("p flag", argv[0]);
                break;
            case 'm':
                manual_deal = 1;
                argc_count++;
                break; 
            case 'h':
                help_msg(argv[0]); 
                exit(EXIT_FAILURE);
            case '?':  
                printf("Error: Unknown option '-%c' received.\n", optopt); 
                exit(EXIT_FAILURE);
        }
    }

    //Check for invalid, additional arguments
    for(int i = argc_count; i < argc; i++){
        printf("Error: Additional argument passed '%s'.\n", argv[i]);
        help_msg(argv[0]);
        exit(EXIT_FAILURE);
    }

    //Setup initial card array with every card appearing once 00 - 93
    card card_arr[n_cards];
    for(int i = 0; i < n_cards; i++){
        card_arr[i].value = i%10;
        card_arr[i].suit = i/10;
    }

    //Either shuffle array to simulate random deal, or allow manual deal
    if(manual_deal){
        set_card_arr(card_arr);
    }else{
        shuffle_card_arr(card_arr, n_cards);
    }

    //Setup and initialize array of players given the card_arr
    player players[n_players];
    init_players(players, n_players, card_arr);

    //Either simulate game of bots, or players vs bot
    if(simulation){
        float avg = 0;
        for(int i = 0; i < simulation_count; i++){
            avg += simulate(card_arr, players);
            init_players(players, n_players, card_arr);
            shuffle_card_arr(card_arr, n_cards);
        }
        printf("Average Evaluation over %d simulations: %f\n", simulation_count, avg/simulation_count);
        exit(EXIT_SUCCESS);
    }else if(pvb){
        for(int i = 0; i < n_players; i++){
            if(i != bot_pos){
                players[i].bot = 0;
            }
        }
        int final_score = run_game(card_arr, players);
        printf("Final Score: %d\n", final_score);
        exit(EXIT_SUCCESS);
    }
    exit(EXIT_FAILURE);
}
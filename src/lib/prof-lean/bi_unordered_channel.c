//
// Created by ax4 on 7/29/19.
//

#include "bi_unordered_channel.h"

void bi_unordered_channel_push(bi_unordered_channel_t *ch, channel_direction_t dir, s_element_t *e)
{
    if (dir == channel_direction_forward) {
        unordered_stack_push(&ch->forward_stack, e);
    } else {
        unordered_stack_push(&ch->backward_stack, e);
    }
}


s_element_t * bi_unordered_channel_pop(bi_unordered_channel_t *ch, channel_direction_t dir)
{
    if (dir == channel_direction_forward) {
        return unordered_stack_pop(&ch->forward_stack);
    } else {
        return unordered_stack_pop(&ch->backward_stack);
    }
}

void bi_unordered_channel_steal(bi_unordered_channel_t *ch, channel_direction_t dir)
{
    if (dir == channel_direction_forward) {
        unordered_stack_steal(&ch->forward_stack);
    } else {
        unordered_stack_steal(&ch->backward_stack);
    }
}


void bi_unordered_channel_init(bi_unordered_channel_t ch){
    unordered_stack_init(&ch.forward_stack);
    unordered_stack_init(&ch.backward_stack);
}


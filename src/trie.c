/*
 * Nitro
 *
 * trie.c - Prefix trie for efficient frame dispatch to sub()'d sockets
 *
 *  -- LICENSE --
 *
 * Copyright 2013 Bump Technologies, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY BUMP TECHNOLOGIES, INC. ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BUMP TECHNOLOGIES, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Bump Technologies, Inc.
 *
 */
#include "trie.h"
#include "frame.h"

void nitro_prefix_trie_search(
    nitro_prefix_trie_node *t, const uint8_t *rep, uint8_t length,
    nitro_prefix_trie_search_callback cb, void *baton) {
    if (!t || t->length > length || memcmp(t->rep, rep, t->length)) {
        return;
    }

    if (t->members) {
        cb(t->rep, t->length, t->members, baton);
    }

    if (t->length < length) {
        uint8_t n = rep[t->length];
        nitro_prefix_trie_node *next = t->subs[n];
        nitro_prefix_trie_search(next, rep, length, cb, baton);
    }
}

void nitro_prefix_trie_add(nitro_prefix_trie_node **t,
                           const uint8_t *rep, uint8_t length, void *ptr) {
    nitro_prefix_trie_node *n, *on;

    if (!*t) {
        ZALLOC(*t);

        if (length) {
            (*t)->length = length;
            (*t)->rep = malloc(length);
            memmove((*t)->rep, rep, length);
        }
    }

    on = n = *t;

    if (n->length < length && !memcmp(n->rep, rep, n->length)) {
        uint8_t c = rep[n->length];
        nitro_prefix_trie_add(&n->subs[c], rep, length, ptr);
    } else {
        // alloc
        nitro_prefix_trie_mem *m;
        ZALLOC(m);
        m->ptr = ptr;

        if (n->length == length && !memcmp(n->rep, rep, length)) {
            DL_APPEND(n->members, m);
        } else {
            ZALLOC(n);
            n->length = length;
            n->rep = malloc(length);
            memmove(n->rep, rep, length);
            DL_APPEND(n->members, m);

            if (n->length < on->length && !memcmp(on->rep, n->rep, n->length)) {
                *t = n;
                n->subs[on->rep[length]] = on;
            } else if (n->length > on->length && !memcmp(on->rep, n->rep, on->length)) {
                *t = on;
                on->subs[n->rep[on->length]] = n;
            } else {
                int i;

                for (i = 0; i < length && on->rep[i] == n->rep[i]; i++) {}

                nitro_prefix_trie_node *parent;
                ZALLOC(parent);
                parent->length = i;
                parent->rep = malloc(parent->length);
                memmove(parent->rep, rep, parent->length);
                parent->subs[rep[parent->length]] = n;
                parent->subs[on->rep[parent->length]] = on;
                *t = parent;
            }
        }
    }
}

int nitro_prefix_trie_del(nitro_prefix_trie_node *t,
                          const uint8_t *rep, uint8_t length, void *ptr) {
    if (!t || t->length > length || memcmp(t->rep, rep, t->length)) {
        return -1;
    }

    if (t->length < length) {
        uint8_t n = rep[t->length];
        nitro_prefix_trie_node *next = t->subs[n];
        return nitro_prefix_trie_del(next, rep, length, ptr);
    } else {
        nitro_prefix_trie_mem *m;

        for (m = t->members; m && ptr != m->ptr; m = m->next) {}

        if (m) {
            DL_DELETE(t->members, m);
            free(m);
            return 0;
        }
    }

    return -1;
}

void nitro_prefix_trie_destroy(nitro_prefix_trie_node *t) {
    if (!t) {
        return;
    }

    assert(t->members == NULL);
    int i;

    for (i = 0; i < 256; i++) {
        if (t->subs[i]) {
            nitro_prefix_trie_destroy(t->subs[i]);
        }
    }

    free(t->rep);
    free(t);
}

#if 0
static void print_trie(nitro_prefix_trie_node *t, int c) {
    int x;

    for (x = 0; x < c; x++) {
        printf(" ");
    }

    printf("%s:%d:%d", t->rep, t->length, t->members ? 1 : 0);
    printf("\n");
    int i;

    for (i = 0; i < 256; i++) {
        if (t->subs[i]) {
            for (x = 0; x < c + 1; x++) {
                printf(" ");
            }

            printf("%d:\n", i);
            print_trie(t->subs[i], c + 2);
        }
    }
}

void callback(uint8_t *pfx, uint8_t length, nitro_prefix_trie_mem *members, void *baton) {
    printf("got: %s\n", (char *)pfx);
}

int main(int argc, char **argv) {
    nitro_prefix_trie_node *root = NULL;
    nitro_prefix_trie_search(root, (uint8_t *)"bar", 3, callback, NULL);
    nitro_prefix_trie_add(&root, (uint8_t *)"bar", 3, NULL);
    printf("after adding bar ---\n");
    print_trie(root, 0);
    nitro_prefix_trie_add(&root, (uint8_t *)"b", 1, NULL);
    printf("after adding b ---\n");
    print_trie(root, 0);
    nitro_prefix_trie_add(&root, (uint8_t *)"bark", 4, NULL);
    printf("after adding bark---\n");
    print_trie(root, 0);
    nitro_prefix_trie_add(&root, (uint8_t *)"baz", 3, NULL);
    printf("after adding baz---\n");
    print_trie(root, 0);
    printf("search for bark ===\n");
    nitro_prefix_trie_search(root, (uint8_t *)"bark", 4, callback, NULL);
    printf("search for baz ===\n");
    nitro_prefix_trie_search(root, (uint8_t *)"baz", 3, callback, NULL);
    nitro_prefix_trie_del(root, (uint8_t *)"bar", 3, NULL);
    printf("search for bark (post delete) ===\n");
    nitro_prefix_trie_search(root, (uint8_t *)"bark", 4, callback, NULL);
    return 0;
}
#endif /* 0 */

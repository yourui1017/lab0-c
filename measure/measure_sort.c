#include "measure_sort.h"
#include "list.h"
/* Ref: https://github.com/python/cpython/blob/main/Objects/listsort.txt */
enum Mode {
    Random = 1,
    Descend,
    Ascend,
    Ascend3,
    AscendPlus,
    AscendPercent,
    Duplicate,
    Equal,
    Worst,
};

/* Distribution 1: random data */
static void random_string(char *s, size_t charlen)
{
    for (int i = 0; i < charlen; i++)
        s[i] = charset[rand() % 26];
}

/* Distribution 2, 3: descending/ascending data */
static void monotonic_string(char *s, size_t charlen, int idx)
{
    for (int i = 0; i < charlen; i++) {
        s[charlen - 1 - i] = charset[idx % 26];
        idx /= 26;
    }
}

static void duplicate_string(char *s, size_t charlen, int idx)
{
    for (int i = 0; i < charlen; i++)
        s[i] = charset[idx % 4];
}

static void equal_string(char *s, size_t charlen)
{
    for (int i = 0; i < charlen; i++)
        s[i] = charset[0];
}

static void worstcase_string(char *s, size_t charlen, int idx)
{
    /* descending for first half part of list */
    monotonic_string(s, charlen, (idx < charlen / 2) ? charlen - 1 - idx : idx);
}

static void ascending_plus_string(char *s, size_t charlen, int idx)
{
    if (idx < charlen - 10)
        monotonic_string(s, charlen, idx);
    else
        random_string(s, charlen);
}

static void ascending_three_swap(element_t *space, size_t charlen, int samples)
{
    char *tmp = malloc(sizeof(char) * charlen);
    for (int i = 0; i < 3; i++) {
        int idx1 = rand() % samples, idx2 = rand() % samples;

        /* swap */
        strncpy(tmp, (space + idx1)->value, charlen);
        strncpy((space + idx1)->value, (space + idx2)->value, charlen);
        strncpy((space + idx2)->value, tmp, charlen);
    }
    free(tmp);
}

// static void ascending_onepercent_string(element_t *space, size_t charlen, int
// samples)
// {
//     for (int i=0; i<samples/100; i++) {
//         int idx = rand() % samples;

//     }
// }

static void create_sample(struct list_head *head,
                          element_t *space,
                          int samples,
                          size_t charlen,
                          size_t mode)
{
    printf("Creating sample for\n");
    enum Mode m = mode;
    for (int i = 0; i < samples; i++) {
        element_t *elem = space + i;
        elem->value = malloc(sizeof(char) * (charlen + 1));
        elem->seq = i;
        switch (m) {
        case Random:
            random_string(elem->value, charlen);
            break;
        case Descend:
            monotonic_string(elem->value, charlen, samples - 1 - i);
            break;
        case Equal:
            equal_string(elem->value, charlen);
            break;
        case Duplicate:
            duplicate_string(elem->value, charlen, i);
            break;
        case Worst:
            worstcase_string(elem->value, charlen, i);
            break;
        case AscendPlus:
            ascending_plus_string(elem->value, charlen, i);
            break;
        /* default is ascending data */
        default:
            monotonic_string(elem->value, charlen, i);
            break;
        }
        elem->value[charlen] = '\0';
    }

    /* if swap is needed */
    switch (m) {
    case Ascend3:
        ascending_three_swap(space, charlen, samples);
        break;
    default:
        break;
    }

    for (int i = 0; i < samples; i++) {
        element_t *elem = space + i;
        list_add_tail(&elem->list, head);
    }
}

static void free_sample(element_t *space, int samples)
{
    for (int i = 0; i < samples; i++) {
        element_t *elem = space + i;
        free(elem->value);
        list_del(&elem->list);
    }

    free(space);
}

static void copy_list(struct list_head *from,
                      struct list_head *to,
                      element_t *space,
                      size_t charlen)
{
    if (list_empty(from))
        return;

    // cppcheck-suppress nullPointer
    element_t *entry = list_entry(from, element_t, list);
    list_for_each_entry (entry, from, list) {
        element_t *copy = space++;
        copy->value = malloc(sizeof(char) * (charlen + 1));
        copy->seq = entry->seq;
        strncpy(copy->value, entry->value, charlen + 1);
        list_add_tail(&copy->list, to);
    }
}

// static void print_list(struct list_head *head)
// {
//     element_t *elem = NULL;
//     list_for_each_entry(elem, head, list)
//         printf("%d:%s --> ", elem->seq, elem->value);
//     printf("\n");
// }

int compare(void *priv, const struct list_head *a, const struct list_head *b)
{
    if (a == b)
        return 0;

    // cppcheck-suppress nullPointer
    int res = strcmp(list_entry(a, element_t, list)->value,
                     // cppcheck-suppress nullPointer
                     list_entry(b, element_t, list)->value);

    if (priv)
        *((int *) priv) += 1;

    return res;
}

bool check_list(struct list_head *head, int count)
{
    if (list_empty(head))
        return 0 == count;

    struct list_head *node, *safe;
    long ctr = 0;
    list_for_each_safe (node, safe, head) {
        ctr++;
    }
    int unstable = 0;
    list_for_each_safe (node, safe, head) {
        if (node->next != head) {
            if (compare(NULL, node, safe) > 0) {
                fprintf(stderr, "\nERROR: Wrong order\n");
                return false;
            }
            if (!compare(NULL, node, safe) &&
                // cppcheck-suppress nullPointer
                list_entry(node, element_t, list)->seq >
                    // cppcheck-suppress nullPointer
                    list_entry(safe, element_t, list)->seq)
                unstable++;
        }
    }
    if (unstable) {
        fprintf(stderr, "\nERROR: unstable %d\n", unstable);
        return false;
    }

    if (ctr != SAMPLES) {
        fprintf(stderr, "\nERROR: Inconsistent number of elements: %ld\n", ctr);
        return false;
    }
    return true;
}


int main(void)
{
    struct list_head sample_head, warmdata_head, testdata_head;
    int count;
    int nums = SAMPLES;

    /* Assume ASLR */
    srand((uintptr_t) &main);

    test_t tests[] = {
        {.name = "timsort", .impl = timsort},
        // {.name = "list_sort", .impl = list_sort},
        {NULL, NULL},
    };
    test_t *test = tests;

    INIT_LIST_HEAD(&sample_head);

    element_t *samples = malloc(sizeof(*samples) * SAMPLES);
    element_t *warmdata = malloc(sizeof(*warmdata) * SAMPLES);
    element_t *testdata = malloc(sizeof(*testdata) * SAMPLES);

    create_sample(&sample_head, samples, nums, CHAR_LEN, Worst);

    while (test->impl) {
        printf("==== Testing %s ====\n", test->name);
        /* Warm up */
        INIT_LIST_HEAD(&warmdata_head);
        INIT_LIST_HEAD(&testdata_head);
        copy_list(&sample_head, &testdata_head, testdata, CHAR_LEN);
        copy_list(&sample_head, &warmdata_head, warmdata, CHAR_LEN);
        test->impl(&count, &warmdata_head, compare);

        /* Test */
        count = 0;
        test->impl(&count, &testdata_head, compare);
        printf("  Comparisons:    %d\n", count);
        printf("  List is %s\n",
               check_list(&testdata_head, nums) ? "sorted" : "not sorted");
        test++;
    }

    printf("freeing sample\n");
    free_sample(samples, nums);
    free_sample(warmdata, nums);
    free_sample(testdata, nums);

    return 0;
}
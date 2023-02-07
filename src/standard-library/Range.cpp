#include "Range.h"

// TODO: Iterable interface
const char * getStandardLibraryRangeCode() {
    return R"""(
class Range {
    current: Int
    start: Int
    end: Int
    steps: Int

    Range(end: Int) {
        self.current = 0
        self.start = 0
        self.end = end
        self.steps = 1
    }

    Range(start: Int, end: Int) {
        self.current = start
        self.start = start
        self.end = end
        self.steps = 1
    }

    Range(start: Int, end: Int, steps: Int) {
        self.current = start
        self.start = start
        self.end = end
        self.steps = steps
    }

    hasNext(): Bool {
        return self.current < self.end
    }

    getNext(): Int {
        var result = self.current
        self.current = self.current + self.steps
        return result
    }
}

// TODO: Change this to return Iterable
// range(end: Int): Range {
//     return new Range(end)
// }

// TODO: Change this to return Iterable
range(start: Int, end: Int): Range {
    return new Range(start, end)
}

// // TODO: Change this to return Iterable
// range(start: Int, end: Int, steps: Int): Range {
//     return new Range(start, end, steps)
// }
)""";
}
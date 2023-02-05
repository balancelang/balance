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

    hasNext(): Bool {
        return self.current < self.end
    }

    getNext(): Int {
        var result = self.current
        self.current = self.current + self.steps
        return result
    }
}
)""";
}
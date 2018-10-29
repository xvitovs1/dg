#ifndef _DG_DISJUNCTIVE_INTERVAL_MAP_H_
#define _DG_DISJUNCTIVE_INTERVAL_MAP_H_

#include "dg/analysis/Offset.h"
#include <cassert>
#include <map>
#include <set>

namespace dg {
namespace analysis {
namespace rd {

///
// Mapping of disjunctive discrete intervals of values
// to sets of ValueT.
template <typename ValueT, typename IntervalValueT = Offset>
class DisjunctiveIntervalMap {
public:
    template <typename T = int64_t>
    struct Interval {
        T start;
        T end;

        Interval(T s, T e) : start(s), end(e) {
            assert(s <= e && "Invalid interval");
        }

        // total order on intervals so that we can insert them
        // to std containers. We want to compare them only
        // according to the start value.
        bool operator<(const Interval& I) const {
            return start < I.start;
        }

        bool operator==(const Interval& I) const {
            return start == I.start && end == I.end;
        }

        bool operator!=(const Interval& I) const {
            return !operator==(I);
        }
    };

    using IntervalT = Interval<IntervalValueT>;
    using ValuesT = std::set<ValueT>;
    using MappingT = std::map<IntervalT, ValuesT>;

    ///
    // Return true if the mapping is updated anyhow
    // (intervals split, value added).
    bool add(const IntervalValueT start, const IntervalValueT end,
             const ValueT& val) {
        return add(IntervalT(start, end), val);
    }

    bool add(const IntervalT& I, const ValueT& val) {
        return _add(I, val, false);
    }

    bool update(const IntervalValueT start, const IntervalValueT end,
                const ValueT& val) {
        return update(IntervalT(start, end), val);
    }

    bool update(const IntervalT& I, const ValueT& val) {
        return _add(I, val, true);
    }

    // return true if some intervals from the map
    // has a overlap with I
    bool overlaps(const IntervalT& I) const {
        if (_mapping.empty())
            return false;

        auto ge = _find_ge(I);
        if (ge == _mapping.end()) {
            auto last = _get_last();
            return last->first.end >= I.start;
        } else {
            return ge->first.start <= I.end;
        }
    }

    bool overlaps(IntervalValueT start, IntervalValueT end) const {
        return overlaps(IntervalT(start, end));
    }

    // return true if the map has an entry for
    // each single byte from the interval I
    bool overlapsFull(const IntervalT& I) const {
        if (_mapping.empty()) {
            return false;
        }

        auto ge = _find_ge(I);
        if (ge == _mapping.end()) {
            auto last = _get_last();
            return last->first.end >= I.end;
        } else {
            if (ge->first.start > I.start) {
                if (ge == _mapping.begin())
                    return false;
                auto prev = --ge;
                if (prev->first.end != ge->first.start - 1)
                    return false;
            }

            IntervalValueT last_end = ge->first.end;
            while (ge->first.end < I.end) {
                ++ge;
                if (ge == _mapping.end())
                    return false;

                if (ge->first.start != last_end + 1)
                    return false;

                last_end = ge->first.end;
            }

            return true;
        }
    }

    bool overlapsFull(IntervalValueT start, IntervalValueT end) const {
        return overlapsFull(IntervalT(start, end));
    }

    bool empty() const { return _mapping.empty(); }
    size_t size() const { return _mapping.size(); }

    typename MappingT::iterator begin() { return _mapping.begin(); }
    typename MappingT::const_iterator begin() const { return _mapping.begin(); }
    typename MappingT::iterator end() { return _mapping.end(); }
    typename MappingT::const_iterator end() const { return _mapping.end(); }

#ifndef NDEBUG
    friend std::ostream& operator<<(std::ostream& os, const DisjunctiveIntervalMap<ValueT, IntervalValueT>& map) {
        os << "{";
        for (const auto& pair : map) {
            if (pair.second.empty())
                continue;

            os << "{ ";
            os << pair.first.start << "-" << pair.first.end;
            os << ": " << *pair.second.begin();
            os << " }, ";
        }
        os << "}";
        return os;
    }
#endif

private:

    // Split interval [a,b] to two intervals [a, where] and [where + 1, b].
    // Each of the new intervals has a copy of the original set associated
    // to the original interval.
    // Returns the new lower interval
    // XXX: could we pass just references to the iterator?
    template <typename IteratorT, typename HintIteratorT>
    IteratorT splitIntervalHint(IteratorT I, IntervalValueT where,
                                HintIteratorT hint) {
        auto interval = I->first;
        auto values = std::move(I->second);

        assert(interval.start != interval.end && "Cannot split such interval");
        assert(interval.start <= where && where <= interval.end
               && "Value 'where' must lie inside the interval");
        assert(where < interval.end && "The second interval would be empty");

        // remove the original interval and replace it with
        // two splitted intervals
        _mapping.erase(I);
        auto ret =
        _mapping.emplace_hint(hint, IntervalT(interval.start, where), values);
        _mapping.emplace_hint(hint, IntervalT(where + 1, interval.end),
                              std::move(values));
        return ret;
    }

    bool splitExtBorders(const IntervalT& I) {
        assert(!_mapping.empty());

        auto ge = _mapping.lower_bound(I);
        if (ge == _mapping.end()) {
            // the last interval must start somewhere to the
            // left from our new interval
            auto last = _get_last();
            assert(last->first.start < I.start);

            bool changed = false;
            if (last->first.end > I.end) {
                last = splitIntervalHint(last, I.end, ge);
                changed |= true;
            }

            if (last->first.end >= I.start) {
                splitIntervalHint(last, I.start - 1, ge);
                changed |= true;
            }
            return changed;
        } else {
            // we found an interval starting at I.start
            // or later
            assert(ge->first.start >= I.start);
            if (ge->first.start <= I.end
                && ge->first.end > I.end) {
                splitIntervalHint(ge, I.end, _mapping.end());
                return true;
            }
        }

        return false;
    }

    template <typename IteratorT>
    bool _addValue(IteratorT I, ValueT val, bool update) {
        if (update) {
            if (I->second.size() == 1 && I->second.count(val) > 0)
                return false;

            I->second.clear();
            I->second.insert(val);
            return true;
        }

        return I->second.insert(val).second;
    }

    // If the boolean 'update' is set to true, the value
    // is not added, but rewritten
    bool _add(const IntervalT& I, const ValueT& val, bool update = false) {
        if (_mapping.empty()) {
            _mapping.emplace(I, ValuesT{val});
            return true;
        }

        // fast path
       //auto fastit = _mapping.lower_bound(I);
       //if (fastit != _mapping.end() &&
       //    fastit->first == I) {
       //    return _addValue(fastit, val, update);
       //}

        // XXX: pass the lower_bound iterator from the fast path
        // so that we do not search the mapping again
        bool changed = splitExtBorders(I);
        _check();

        // splitExtBorders() arranged the intervals such
        // that some interval starts with ours
        // and some interval ends with ours.
        // Now we just create new intervals in the gaps
        // and add values to the intervals that we have

        // FIXME: splitExtBorders() can return the iterator,
        // so that we do not need to search the map again.
        auto it = _find_ge(I);
        assert(!changed || it != _mapping.end());

        // we do not have any overlapping interval
        if (it == _mapping.end()
            || I.end < it->first.start) {
            assert(!overlaps(I) && "Bug in add() or in overlaps()");
            _mapping.emplace(I, ValuesT{val});
            return true;
        }

        auto rest = I;
        assert(rest.end >= it->first.start); // we must have some overlap here
        while (it != _mapping.end()) {
            if (rest.start < it->first.start) {
                // add the gap interval
                _mapping.emplace_hint(it, IntervalT(rest.start, it->first.start - 1),
                                      ValuesT{val});
                rest.start = it->first.start;
                changed = true;
            } else {
                // update the existing interval and shift
                // to the next interval
                assert(rest.start == it->first.start);
                changed |= _addValue(it, val, update);
                if (it->first.end == rest.end)
                    break;

                rest.start = it->first.end + 1;
                ++it;

                // our interval spans to the right
                // after the last covered interval
                if (it == _mapping.end()
                    || it->first.start > rest.end) {
                    _mapping.emplace_hint(it, rest, ValuesT{val});
                    changed = true;
                    break;
                }
            }
        }

        _check();
        return changed;
    }

    // find the elements starting at
    // or right to the interval
    typename MappingT::iterator _find_ge(const IntervalT& I) {
        // lower_bound = lower upper bound
        return _mapping.lower_bound(I);
    }

    typename MappingT::const_iterator _find_ge(const IntervalT& I) const {
        return _mapping.lower_bound(I);
    }

    typename MappingT::iterator _get_last() {
        assert(!_mapping.empty());
        return (--_mapping.end());
    }

    typename MappingT::const_iterator _get_last() const {
        assert(!_mapping.empty());
        return (--_mapping.end());
    }

    void _check() const {
#ifndef NDEBUG
        // check that the keys are disjunctive
        auto it = _mapping.begin();
        auto last = it->first;
        ++it;
        while (it != _mapping.end()) {
            assert(last.start <= last.end);
            // this one is nontrivial (the other assertions
            // should be implied by the Interval and std::map propertis)
            assert(last.end < it->first.start);
            assert(it->first.start <= it->first.end);

            last = it->first;
            ++it;
        }
#endif // NDEBUG
    }

    MappingT _mapping;
};


} // namespace rd
} // namespace analysis
} // namespace dg

#endif // _DG_DISJUNCTIVE_INTERVAL_MAP_H_

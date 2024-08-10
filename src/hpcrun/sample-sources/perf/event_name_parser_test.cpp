// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

#include "event_name_parser.h"

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <vector>

struct EventNameParseParam {
  EventNameParseParam(const char* input, std::initializer_list<const char*> strevents,
                      const char* strhow_often = nullptr)
      : input(input), how_often(strhow_often == nullptr
                                    ? (testing::Matcher<const char*>)testing::IsNull()
                                    : testing::StrEq(strhow_often)) {
    events.reserve(strevents.size());
    for (const char* e : strevents)
      events.push_back(testing::StrEq(e));
  }
  const char* input;
  std::vector<testing::Matcher<const char*>> events;
  testing::Matcher<const char*> how_often;
};

class EventNameParserTest : public testing::TestWithParam<EventNameParseParam> {};
TEST_P(EventNameParserTest, PerfUtilParseEventname) {
  perf_event_name_t obj;
  int result = perf_util_parse_eventname(GetParam().input, &obj);
  EXPECT_EQ(result, GetParam().events.size());
  EXPECT_THAT(obj.how_often, GetParam().how_often);
  EXPECT_EQ(obj.skid, nullptr);

  std::vector<const char*> events;
  events.reserve(obj.num_events);
  for (int i = 0; i < obj.num_events; ++i)
    events.push_back(obj.event_names[i]);
  EXPECT_THAT(events, testing::ElementsAreArray(GetParam().events));
}
INSTANTIATE_TEST_SUITE_P(
    , EventNameParserTest,
    testing::Values(
        EventNameParseParam("event_name", {"event_name"}),
        EventNameParseParam{"event1,event2", {"event1", "event2"}},
        EventNameParseParam{"event_name@100", {"event_name"}, "100"},
        EventNameParseParam{"event_name@f100", {"event_name"}, "f100"},
        EventNameParseParam{"'{event1,event2}'", {"event1", "event2"}},
        EventNameParseParam{"'{event1,event2}'@100", {"event1", "event2"}, "100"},
        EventNameParseParam{"'{event1,event2}'@f10", {"event1", "event2"}, "f10"},
        EventNameParseParam{"event_name:p", {"event_name:p"}},
        EventNameParseParam{"event_name:pp@100", {"event_name:pp"}, "100"},
        EventNameParseParam{"event_name:P@f2000", {"event_name:P"}, "f2000"},
        EventNameParseParam{"event_name:ppp@f100", {"event_name:ppp"}, "f100"},
        EventNameParseParam{"'{event1,event2}':ppp", {"event1", "event2"}},
        EventNameParseParam{"'{event1,event2}':ppp@100", {"event1", "event2"}, "100"},
        EventNameParseParam{"'{event1,event2}':ppp@f100", {"event1", "event2"}, "f100"},
        EventNameParseParam{"module::event:p_modifier=1:p_umask=2:123_bytes",
                            {"module::event:p_modifier=1:p_umask=2:123_bytes"}},
        EventNameParseParam{"module::event:p_modifier=1:p_umask=2:123_bytes:p",
                            {"module::event:p_modifier=1:p_umask=2:123_bytes:p"}},
        EventNameParseParam{"module::event:p_modifier=1:p_umask=2:123_bytes:pp",
                            {"module::event:p_modifier=1:p_umask=2:123_bytes:pp"}},
        EventNameParseParam{"module::event:p_modifier=1:p_umask=2:123_bytes:ppp",
                            {"module::event:p_modifier=1:p_umask=2:123_bytes:ppp"}},
        EventNameParseParam{"module::event:p_modifier=1:p_umask=2:123_bytes@100000",
                            {"module::event:p_modifier=1:p_umask=2:123_bytes"},
                            "100000"},
        EventNameParseParam{"module::event:p_modifier=1:p_umask=2:123_bytes:p@100000",
                            {"module::event:p_modifier=1:p_umask=2:123_bytes:p"},
                            "100000"},
        EventNameParseParam{"module::event:p_modifier=1:p_umask=2:123_bytes:pp@100000",
                            {"module::event:p_modifier=1:p_umask=2:123_bytes:pp"},
                            "100000"},
        EventNameParseParam{"module::event:p_modifier=1:p_umask=2:123_bytes:ppp@100000",
                            {"module::event:p_modifier=1:p_umask=2:123_bytes:ppp"},
                            "100000"},
        EventNameParseParam{"module::event:p_modifier=1:p_umask=2:123_bytes@f100000",
                            {"module::event:p_modifier=1:p_umask=2:123_bytes"},
                            "f100000"}));

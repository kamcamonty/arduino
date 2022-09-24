#include "./funshield.h"

constexpr int button_pins[] = { button1_pin, button2_pin, button3_pin };

constexpr int button_count = sizeof(button_pins) / sizeof(button_pins[0]);
constexpr int display_positions_count = 4;

constexpr unsigned int button_press_tolerance = 0;

constexpr unsigned int button_index_start_stop = 0;
constexpr unsigned int button_index_lap = 1;
constexpr unsigned int button_index_reset = 2;

constexpr byte display_position_all = 0b00001111;
constexpr byte display_glyph_all_off = 0b11111111;

constexpr byte display_dot_glyph = 0b01111111;

int system_base = 10;
int decimal_places = 1;

int seconds_to_millis_exponent = 3;

enum StopWatchState { stopped, running, lapped };

int power(int base, int exponent) {
  int result = 1;
  for (int i = 0; i < exponent; ++i)
  {
    result *= base;
  }
  return result;
}

class Button {
  private:
    unsigned int number_;
    bool status_;
    unsigned long next_action_ = 0;
    bool waiting_to_action_ = false;

    bool get_current_status() {
      return digitalRead(button_pins[number_]) == LOW;
    }

    bool time_over_threshold() {
      return next_action_ <= millis();
    }

  public:
    Button() {}
    Button(int button): number_ (button), status_(false), waiting_to_action_(false) { }

    bool activated() {
      bool current_status = get_current_status();
      bool newly_pressed = !status_ && current_status;
      bool still_pressed = status_ && current_status;

      status_ = current_status;

      if (newly_pressed) {
        next_action_ = millis() + button_press_tolerance;
        waiting_to_action_ = true;
      }

      if (still_pressed && time_over_threshold() && waiting_to_action_) {
        waiting_to_action_ = false;
        return true;
      }

      return false;
    };

    void initialize() {
      pinMode(button_pins[number_], INPUT);
    }
};

class Display {
  public:
    void show_symbol(byte position_mask, byte glyph_mask) {
      digitalWrite(latch_pin, LOW);
      shiftOut(data_pin, clock_pin, MSBFIRST, glyph_mask);
      shiftOut(data_pin, clock_pin, MSBFIRST, position_mask);
      digitalWrite(latch_pin, HIGH);
    };

    byte compute_position_mask(int i) {
      return 1 << (display_positions_count - i - 1);
    }

    byte create_mask_with_dot(byte digit_glyph) {
      return digit_glyph & display_dot_glyph;
    }

    void show_digit(byte position_mask, int digit, bool display_dot = false) {
      byte glyph_mask = display_dot ?  create_mask_with_dot(digits[digit]) : digits[digit];
      show_symbol(position_mask, glyph_mask);
    };

    void show_number_digit(int counter, int order, bool display_dot = false) {
      int current_digit = (counter / power(system_base, order)) % system_base;
      byte position_mask = compute_position_mask(order);

      show_digit(position_mask, current_digit, display_dot);
    }

    void initialize() {
      pinMode(latch_pin, OUTPUT);
      pinMode(clock_pin, OUTPUT);
      pinMode(data_pin, OUTPUT);
      //show_symbol(display_position_all, display_glyph_all_off);
    };
};

class NumericDisplay : public Display {
  private:
    int number_ = 0;
    int position_ = 0;
    int highest_order_ = 0;
    bool dot_presence_ = false;
    int dot_position_ = 0;

  public:
    void set_number(int new_value) {
      number_ = new_value;
      update_order();
    }

    void update_order() {
      int temp_number = number_;
      highest_order_ = 0;
      while (temp_number > system_base) {
        temp_number = (temp_number / system_base);
        ++highest_order_;
      }

      if (highest_order_ < dot_position_) {
        highest_order_ = dot_position_;
      }
    }

    void set_decimal_places(bool enable, int places = 0) {
      if (!enable) {
        dot_presence_ = false;
        dot_position_ = 0;
      } else {
        dot_presence_ = true;
        dot_position_ = places;
      }
    }

    void update_position() {
      ++position_;
      position_ = position_ % display_positions_count;
    }

    void display_next() {
      if (position_ <= highest_order_) {
        bool should_display_dot = dot_presence_ && (dot_position_ == position_);
        show_number_digit(number_, position_, should_display_dot);
      } else {
        show_symbol(position_, display_glyph_all_off);
      }
      update_position();
    }
};

class StopWatch {
  private:
    long start_time_stamp_;
    long to_display_time_;
    long measured_time_;
    StopWatchState state_ = stopped;
    bool should_set_start_time = true;

    long measure_time() {
      return millis() - start_time_stamp_;
    }

  public:
    void start_stop() {
      switch (state_)
      {
        case stopped:
          state_ = running;
          if (should_set_start_time) {
            should_set_start_time = false;
            start_time_stamp_ = millis();
          }
          break;
        case running:
          state_ = stopped;
          break;
        case lapped:
          break;
      }
    }

    void lap() {
      switch (state_)
      {
        case running:
          state_ = lapped;
          break;
        case lapped:
          state_ = running;
          break;
        case stopped:
          break;
      }
    }

    void reset() {
      if (state_ == stopped) {
        measured_time_ = 0;
        to_display_time_ = 0;
        should_set_start_time = true;
      }
    }

    void update(long actual_measured_time) {
      if (state_ != stopped) {
        measured_time_ = actual_measured_time;
      }
      if (state_ == running) {
        to_display_time_ = measured_time_;
      }
    }

    bool should_update() {
      long actual_measured_time = measure_time();
      bool should_update = measured_time_ != actual_measured_time;
      if (should_update) {
        update(actual_measured_time);
      }
      return should_update;
    };

    long get_time_to_display() {
      return round_time(to_display_time_);
    }

    long round_time(long millis_to_round){
    return floor(millis_to_round / power(system_base, (seconds_to_millis_exponent - decimal_places)));
      }
};

Button buttons[button_count];
NumericDisplay display;
StopWatch stopWatch;

void setup() {

  for (int i = 0; i < button_count; ++i) {
    buttons[i] = Button(i);
    buttons[i].initialize();
  }

  display.set_decimal_places(true, decimal_places);
  display.set_number(stopWatch.get_time_to_display());
  display.initialize();
}

void loop() {

  if (buttons[button_index_start_stop].activated()) {
    stopWatch.start_stop();
  };

  if (buttons[button_index_lap].activated()) {
    stopWatch.lap();
  };

  if (buttons[button_index_reset].activated()) {
    stopWatch.reset();
  };

  if (stopWatch.should_update()) {
    display.set_number(stopWatch.get_time_to_display());
  }

  display.display_next();
}

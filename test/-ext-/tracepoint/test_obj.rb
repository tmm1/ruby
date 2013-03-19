require 'test/unit'
require '-test-/tracepoint'

class TestTracepointObj < Test::Unit::TestCase
  def test_not_available_from_ruby
    assert_raises ArgumentError do
      TracePoint.trace(:obj_new){}
    end
  end

  def test_tracks_newly_created_objects
    newobjs = Bug.tracepoint_track_newobj{
      99
      'abc'
      v="foobar"
      Object.new
      nil
    }

    assert_equal 2, newobjs.size
    assert_equal 'foobar', newobjs[0]
    assert_equal Object, newobjs[1].class
  end
end

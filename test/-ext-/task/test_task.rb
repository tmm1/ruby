require 'test/unit'
require 'thread'
require '-test-/task'

module Bug
  def self.task_direct_caller_wrapper(*args)
    task_direct_caller(*args)
  end

  def self.task_enqueue_caller_wrapper(*args)
    task_enqueue_caller(*args)
  end
end

class TestTask < Test::Unit::TestCase
  def test_enqueue
    direct, enqueued = [], []

    Bug.task_direct_caller_wrapper(direct)
    Bug.task_enqueue_caller_wrapper(enqueued)

    assert_match     /task_direct_caller_wrapper/,  direct.join
    assert_not_match /task_enqueue_caller_wrapper/, enqueued.join
  end
end

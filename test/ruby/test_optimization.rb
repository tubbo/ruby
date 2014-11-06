require 'test/unit'

class TestRubyOptimization < Test::Unit::TestCase

  BIGNUM_POS_MIN_32 = 1073741824      # 2 ** 30
  if BIGNUM_POS_MIN_32.kind_of?(Fixnum)
    FIXNUM_MAX = 4611686018427387903  # 2 ** 62 - 1
  else
    FIXNUM_MAX = 1073741823           # 2 ** 30 - 1
  end

  BIGNUM_NEG_MAX_32 = -1073741825     # -2 ** 30 - 1
  if BIGNUM_NEG_MAX_32.kind_of?(Fixnum)
    FIXNUM_MIN = -4611686018427387904 # -2 ** 62
  else
    FIXNUM_MIN = -1073741824          # -2 ** 30
  end

  def assert_redefine_method(klass, method, code, msg = nil)
    assert_separately([], <<-"end;")#    do
      class #{klass}
        undef #{method}
        def #{method}(*args)
          args[0]
        end
      end
      #{code}
    end;
  end

  def assert_no_allocation(mesg = "", adjust = 0)
    before = GC.stat(:total_allocated_objects)
    yield
    after = GC.stat(:total_allocated_objects)
    assert_equal before, after - adjust, mesg
  end

  def test_fixnum_plus
    a, b = 1, 2
    assert_equal 3, a + b
    assert_instance_of Fixnum, FIXNUM_MAX
    assert_instance_of Bignum, FIXNUM_MAX + 1

    assert_equal 21, 10 + 11
    assert_redefine_method('Fixnum', '+', 'assert_equal 11, 10 + 11')
  end

  def test_fixnum_minus
    assert_equal 5, 8 - 3
    assert_instance_of Fixnum, FIXNUM_MIN
    assert_instance_of Bignum, FIXNUM_MIN - 1

    assert_equal 5, 8 - 3
    assert_redefine_method('Fixnum', '-', 'assert_equal 3, 8 - 3')
  end

  def test_fixnum_mul
    assert_equal 15, 3 * 5
    assert_redefine_method('Fixnum', '*', 'assert_equal 5, 3 * 5')
  end

  def test_fixnum_div
    assert_equal 3, 15 / 5
    assert_redefine_method('Fixnum', '/', 'assert_equal 5, 15 / 5')
  end

  def test_fixnum_mod
    assert_equal 1, 8 % 7
    assert_redefine_method('Fixnum', '%', 'assert_equal 7, 8 % 7')
  end

  def test_float_plus
    assert_equal 4.0, 2.0 + 2.0
    assert_redefine_method('Float', '+', 'assert_equal 2.0, 2.0 + 2.0')
  end

  def test_float_minus
    assert_equal 4.0, 2.0 + 2.0
    assert_redefine_method('Float', '+', 'assert_equal 2.0, 2.0 + 2.0')
  end

  def test_float_mul
    assert_equal 29.25, 4.5 * 6.5
    assert_redefine_method('Float', '*', 'assert_equal 6.5, 4.5 * 6.5')
  end

  def test_float_div
    assert_in_delta 0.63063063063063063, 4.2 / 6.66
    assert_redefine_method('Float', '/', 'assert_equal 6.66, 4.2 / 6.66', "[Bug #9238]")
  end

  def test_string_length
    assert_equal 6, "string".length
    assert_redefine_method('String', 'length', 'assert_nil "string".length')
  end

  def test_string_size
    assert_equal 6, "string".size
    assert_redefine_method('String', 'size', 'assert_nil "string".size')
  end

  def test_string_empty?
    assert_equal true, "".empty?
    assert_equal false, "string".empty?
    assert_redefine_method('String', 'empty?', 'assert_nil "string".empty?')
  end

  def test_string_plus
    assert_equal "", "" + ""
    assert_equal "x", "x" + ""
    assert_equal "x", "" + "x"
    assert_equal "ab", "a" + "b"
    assert_redefine_method('String', '+', 'assert_equal "b", "a" + "b"')
    require_compile_option(:peephole_optimization)
    assert_no_allocation("String#+", 1) { "a" + "b" }
    s = ""
    assert_no_allocation("String#+", 1) { "b" + s }
    assert_no_allocation("String#+", 1) { s + "b" }
  end

  def test_string_succ
    assert_equal 'b', 'a'.succ
    assert_equal 'B', 'A'.succ
  end

  def test_string_format
    assert_equal '2', '%d' % 2
    assert_redefine_method('String', '%', 'assert_equal 2, "%d" % 2')
    require_compile_option(:peephole_optimization)
    assert_no_allocation("String#%", 1) { '%d' % 2 }
  end

  def test_string_freeze
    assert_redefine_method('String', 'freeze', 'assert_nil "foo".freeze')
    require_compile_option(:peephole_optimization)
    assert_no_allocation { 5.times { "".freeze } }
    assert_equal "foo", "foo".freeze
  end

  def test_string_eq_neq_eqq
    %w(== != ==).each do |m|
      assert_redefine_method('String', m, <<-end)
        assert_equal :b, ("a" #{m} "b").to_sym
        b = 'b'
        assert_equal :b, ("a" #{m} b).to_sym
        assert_equal :b, (b #{m} "b").to_sym
      end
    end
  end

  def test_string_ltlt
    assert_equal "", "" << ""
    assert_equal "x", "x" << ""
    assert_equal "x", "" << "x"
    assert_equal "ab", "a" << "b"
    assert_redefine_method('String', '<<', 'assert_equal "b", "a" << "b"')
    require_compile_option(:peephole_optimization)
    assert_no_allocation("String#<<", 1) { "a" << "b" }
    s = ""
    assert_no_allocation("String#<<", 1) { "a" << s }
    assert_no_allocation("String#<<") { s << "b" }
  end

  def test_array_plus
    assert_equal [1,2], [1]+[2]
    assert_redefine_method('Array', '+', 'assert_equal [2], [1]+[2]')
  end

  def test_array_minus
    assert_equal [2], [1,2] - [1]
    assert_redefine_method('Array', '-', 'assert_equal [1], [1,2]-[1]')
  end

  def test_array_length
    assert_equal 0, [].length
    assert_equal 3, [1,2,3].length
    assert_redefine_method('Array', 'length', 'assert_nil([].length); assert_nil([1,2,3].length)')
  end

  def test_array_empty?
    assert_equal true, [].empty?
    assert_equal false, [1,2,3].empty?
    assert_redefine_method('Array', 'empty?', 'assert_nil([].empty?); assert_nil([1,2,3].empty?)')
  end

  def test_hash_length
    assert_equal 0, {}.length
    assert_equal 1, {1=>1}.length
    assert_redefine_method('Hash', 'length', 'assert_nil({}.length); assert_nil({1=>1}.length)')
  end

  def test_hash_empty?
    assert_equal true, {}.empty?
    assert_equal false, {1=>1}.empty?
    assert_redefine_method('Hash', 'empty?', 'assert_nil({}.empty?); assert_nil({1=>1}.empty?)')
  end

  def test_hash_aref_with
    h = { "foo" => 1 }
    assert_equal 1, h["foo"]
    assert_redefine_method('Hash', '[]', <<-end)
      h = { "foo" => 1 }
      assert_equal "foo", h["foo"]
    end
  end

  def test_hash_aset_with
    h = {}
    assert_equal 1, h["foo"] = 1
    assert_redefine_method('Hash', '[]=', <<-end)
      h = {}
      assert_equal 1, h["foo"] = 1, "assignment always returns value set"
      assert_nil h["foo"]
    end
  end

  class MyObj
    def ==(other)
      true
    end
  end

  def test_eq
    assert_equal true, nil == nil
    assert_equal true, 1 == 1
    assert_equal true, 'string' == 'string'
    assert_equal true, 1 == MyObj.new
    assert_equal false, nil == MyObj.new
    assert_equal true, MyObj.new == 1
    assert_equal true, MyObj.new == nil
  end

  def test_tailcall
    bug4082 = '[ruby-core:33289]'

    option = {
      tailcall_optimization: true,
      trace_instruction: false,
    }
    RubyVM::InstructionSequence.new(<<-EOF, "Bug#4082", bug4082, nil, option).eval
      class #{self.class}::Tailcall
        def fact_helper(n, res)
          if n == 1
            res
          else
            fact_helper(n - 1, n * res)
          end
        end
        def fact(n)
          fact_helper(n, 1)
        end
      end
    EOF
    assert_equal(9131, Tailcall.new.fact(3000).to_s.size, bug4082)
  end

  def test_tailcall_with_block
    bug6901 = '[ruby-dev:46065]'

    option = {
      tailcall_optimization: true,
      trace_instruction: false,
    }
    RubyVM::InstructionSequence.new(<<-EOF, "Bug#6901", bug6901, nil, option).eval
  def identity(val)
    val
  end

  def delay
    -> {
      identity(yield)
    }
  end
    EOF
    assert_equal(123, delay { 123 }.call, bug6901)
  end

  def test_string_opt_str_lit_1_syntax
    require_compile_option(:peephole_optimization)
    s = "a"
    res = true
    assert_no_allocation do
      res &&= s == "a"
      res &&= "a" == s
      res &&= !(s == "b")
      res &&= !("b" == s)
      res &&= s != "b"
      res &&= "b" != s
      res &&= "a" === s
      res &&= s === "a"
      res &&= !("b" === s)
      res &&= !(s === "b")
    end
    assert_equal true, res, res.inspect
  end

  def test_string_opt_str_lit_methods
    require_compile_option(:peephole_optimization)
    {
      split: 3,
      count: 0,
      include?: 0,
      chomp: 1,
      chomp!: 0,
      delete!: 0,
      squeeze: 1,
      squeeze!: 0,
      index: 0,
      rindex: 0,
      start_with?: 0,
      end_with?: 0,
      partition: 3,
      rpartition: 3,
      casecmp: 0,
      match: 7, # opt_str_lit only saved one allocation here...
    }.each do |k,v|
      eval <<-EOF
      s = 'a=b'
      assert_no_allocation('String##{k}', #{v}) { s.#{k}("=") }
      assert_redefine_method('String', '#{k}', 'assert_nil "".#{k}(nil)')
      EOF
    end

    {
      tr!: 0,
      tr_s!: 0,
      tr: 1,
      tr_s: 1,
    }.each do |k,v|
      eval <<-EOF
      s = 'a=b'
      assert_no_allocation('String##{k}', #{v}) { s.#{k}("=", "!") }
      assert_redefine_method('String', '#{k}', 'assert_nil "".#{k}(nil)')
      EOF
    end

    # String#encode(dst)
    s = ''
    assert_no_allocation('String#encode', 1) { s.encode("utf-8") }
    assert_no_allocation('String#encode!') { s.encode!("utf-8") }
    assert_no_allocation('String#force_encoding') { s.force_encoding("utf-8") }

    # String#encode{,!}(dst, src)
    s = ''.b
    assert_no_allocation('String#encode', 1) { s.encode("utf-8", "binary") }
    assert_no_allocation('String#encode!') { s.encode!("utf-8", "binary") }

    assert_no_allocation('String#insert') { s.insert(0, "a") }
    %w(encode encode! force_encoding insert).each do |m|
      assert_redefine_method('String', m, "assert_nil ''.#{m}")
    end


  end

  def test_array_opt_str_lit_methods
    require_compile_option(:peephole_optimization)
    {
      join: 1,
      delete: 0,
      include?: 0,
    }.each do |k,v|
      eval <<-EOF
      x = %w(a b)
      assert_no_allocation('Array##{k}', #{v}) { x.#{k}("m") }
      assert_redefine_method('Array', '#{k}', 'assert_nil [].#{k}(nil)')
      EOF
    end

    x = %w(a b)
    assert_no_allocation('Array#pack', 1) { x.pack('m') }
    # assert_separately uses Array#pack, so we must use assert_normal_exit:
    assert_ruby_status(%w(--disable-gems), <<-EOF, "Array#pack redefine")
      class Array
        undef pack
        def pack(*args); true; end
      end
      exit(%w(a b).pack(false))
    EOF
  end

  def test_hash_opt_str_lit_methods
    require_compile_option(:peephole_optimization)
    {
      delete: 0,
      include?: 0,
      member?: 0,
      has_key?: 0,
      key?: 0,
      fetch: 0,
      # TODO: more more methods, maybe assoc/rassoc
    }.each do |k,v|
      eval <<-EOF
      x = {"x" => 1}
      assert_no_allocation('Hash##{k}') { x.#{k}("x") }
      assert_redefine_method('Hash', '#{k}', 'assert_nil({}.#{k}(nil))')
      EOF
    end
  end

  def test_time_opt_str_lit
    require_compile_option(:peephole_optimization)
    t = Time.now
    t.strftime("%Y") # initialize
    n = 5

    case 1.size
    when 4
      adjust = 2
    when 8
      adjust = 3
    else
      skip "unsupported word size"
    end
    assert_no_allocation("Time#strftime", n * adjust) {
      n.times { t.strftime("%Y") }
    }
    assert_redefine_method('Time', 'strftime',
                           'assert_equal "%Y", Time.now.strftime("%Y")')
  end
end

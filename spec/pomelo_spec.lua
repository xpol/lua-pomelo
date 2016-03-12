require('busted.runner')()
local sleep = require('system').sleep

local pomelo = require('pomelo')

describe('pomelo', function()
  describe('.configure()', function()
    it('configure library without args', function()
      --pomelo.configure()
    end)
    it('configure library with options', function()
      pomelo.configure({log='DISABLE', cafile='cafile', capath='.'})
    end)
  end)
  describe('.version()', function()
    it('returns the libpomelo2 version string', function()
      local v = pomelo.version()
      assert.are.equal('string', type(v))
      assert.is.truthy(v:match('^(%d+)%.(%d+)%.(%d+)%-(.+)$'))
    end)
  end)
  describe('.newClient()', function()
    it('construct a new client instance with default options', function()
      local c = pomelo.newClient({enable_reconn = false})
      assert.are.equal('userdata', type(c))
      assert.is.truthy(tostring(c):match('Client %(0x[%da-f]+%)'))
    end)
    it('construct a new client instance with options', function()
      local c = pomelo.newClient({
        conn_timeout = 30, -- optional, default 30 seconds
        enable_reconn = true, -- optional, default true
        reconn_max_retry = 3, -- optional, 'ALWAYS' or a positive integer. default 'ALWAYS'
        reconn_delay = 2, -- integer, optional, default to 2
        reconn_delay_max = 30, -- integer, optional, default to 30
        reconn_exp_backoff = true,-- boolean, optional, default to true
        transport_name = "TLS" -- 'TCP', 'TLS', 'DUMMY', or an integer id of you customized transport
      })
      assert.are.equal('userdata', type(c))
      assert.is.truthy(tostring(c):match('Client %(0x[%da-f]+%)'))
    end)
  end)
  describe('Client', function()
    local c
    before_each(function()
      c = pomelo.newClient()
    end)
    after_each(function()
      c:close()
      c = nil
    end)
    describe(':connect()', function()
      it('connect to a pomelo server', function()
        assert.are.equal('INITED', c:state())
        assert.is_true(c:connect('127.0.0.1', 3010))
        assert.are.equal('CONNECTING', c:state())
        sleep(1)
        c:poll()
        assert.are.equal('CONNECTED', c:state())
      end)
    end)
    describe(':disconnect()  #try', function()
      it('connect to a pomelo server', function()
        c:connect('127.0.0.5', 3011)
        sleep(1)
        c:poll()
        assert.is_true(c:disconnect())
      end)
    end)
    describe(':request()', function()
      it('send request to pomelo server', function()
        c:connect('127.0.0.1', 3010)
        sleep(1)
        c:poll()
        local callback = spy.new(function()end)
        c:request('connector.entryHandler.entry', '{"name": "test"}', 10, callback)
        sleep(2)
        c:poll()
        assert.spy(callback).was.called(1)
        assert.spy(callback).was.called_with(
          nil,
          {route='connector.entryHandler.entry', msg='{"name": "test"}', timeout=10},
          '{"code":200,"msg":"game server is ok."}'
        )
      end)
    end)
    describe(':notify()', function()
      it('send notify to pomelo server', function()
        c:connect('127.0.0.1', 3010)
        sleep(1)
        c:poll()
        local s = spy.new(function() end)
        c:notify('test.testHandler.notify', '{"content": "test content"}', 10, s)
        sleep(2)
        c:poll()
        assert.spy(s).was.called(1)
      end)
    end)

    describe(':on()', function()
      it('adds event listener to client', function()
        local s = spy.new(function() end)
        c:on('connected', s)
        c:connect('127.0.0.1', 3010)
        sleep(1)
        c:poll()
        c:disconnect()
        sleep(1)
        c:poll()
        c:connect('127.0.0.1', 3010)
        sleep(1)
        c:poll()
        assert.spy(s).was.called(2)
      end)
    end)
    describe(':once()', function()
      it('adds event listener to client', function()
        local s = spy.new(function() end)
        c:once('connected', s)
        c:connect('127.0.0.1', 3010)
        sleep(1)
        c:poll()
        c:disconnect()
        sleep(1)
        c:poll()
        c:connect('127.0.0.1', 3010)
        sleep(1)
        c:poll()
        assert.spy(s).was.called(1)
      end)
    end)
    describe(':off()', function()
      it('removes event listener', function()
        local s = spy.new(function() end)
        c:on('connected', s)
        c:off('connected', s)
        c:connect('127.0.0.1', 3010)
        sleep(1)
        c:poll()
        assert.spy(s).was.called(0)
      end)
    end)
    describe(':listeners()', function()
      it('returns emtpy when no listeners registered', function()
        assert.are.same({}, c:listeners('connected'))
      end)
      it('return the array of listeners registered', function()
        local function f() end
        c:on('connected', f)
        assert.are.same({f}, c:listeners('connected'))
      end)
      it('return the array of listeners registered', function()
        local function f() end
        c:on('connected', f)
        assert.are.same({f}, c:listeners('connected'))
      end)
    end)

    describe(':config()', function()
      it('returns the config of the client', function()
        assert.are.same({
          reconn_delay = 2,
          reconn_delay_max = 30,
          reconn_exp_backoff = true,
          reconn_max_retry = 'ALWAYS',
          transport_name = 'TCP',
          conn_timeout = 30,
          enable_reconn = true
        }, c:config())

        local conf = {
          reconn_delay = 3,
          reconn_delay_max = 15,
          reconn_exp_backoff = false,
          reconn_max_retry = 4,
          transport_name = 'TLS',
          conn_timeout = 15,
          enable_reconn = true
        }
        local c2 = pomelo.newClient(conf)
        assert.are.same(conf, c2:config())
      end)
    end)
    describe(':state()', function()
      it('new created client are in `INITED` state', function()
        assert.are.equal('INITED', c:state())
      end)
      it('call connect() will turn client to CONNECTING', function()
        c:connect('127.0.0.1', 1234)
        assert.are.equal('CONNECTING', c:state())
      end)
    end)
    describe(':poll()', function()
      it('porcess the events in current lua thread', function()
        c:connect('127.0.0.1', 3010)
        local s = spy.new(function()end)
        c:on('connected', s)
        assert.are.equal('CONNECTING', c:state())
        sleep(1)

        assert.are.equal('CONNECTED', c:state()) --> state changed
        assert.spy(s).was.called(0) --> but event not got dispatched until :poll() was called
        c:poll()
        assert.spy(s).was.called(1) --> events dispatched
      end)
    end)
    describe(':close()', function()
      it('destory the client', function()
        c:close()
      end)
      it('it ignore second close call', function()
        c:close()
        c:close()
        c:close()
      end)
    end)
  end)
end)

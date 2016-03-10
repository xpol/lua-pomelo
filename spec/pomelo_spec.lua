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
        sleep(3)
        c:poll()
        assert.is_true(c:disconnect())
        sleep(1)
        c:poll()
      end)
    end)
    describe(':request()', function()
      it('', function()
        c:connect('127.0.0.1', 3010)
        sleep(1)
        c:poll()
        c:request('connector.entryHandler.entry', '{"name": "test"}', 10, function(err, req, res)
          assert.are.equal('connector.entryHandler.entry', req.route)
          assert.are.equal('{"name": "test"}', req.msg)
          assert.are.equal(7, req.timeout)
        end)
        sleep(3)
        c:poll()
        sleep(3)
        c:poll()
      end)
    end)
    describe(':notify()', function()
    end)

    describe(':on()', function()
    end)
    describe(':addListener()', function()
    end)
    describe(':once()', function()
    end)
    describe(':off()', function()
    end)
    describe(':removeListener()', function()
    end)
    describe(':listeners()', function()
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
      it('new created client are in `INITED` state', function()
        c:connect('127.0.0.1', 1234)
        assert.are.equal('CONNECTING', c:state())
      end)
    end)
    describe(':conn_quality()', function()
    end)
    describe(':connQuality()', function()
    end)

    describe(':poll()', function()
    end)

    describe(':close()', function()

    end)
  end)
end)

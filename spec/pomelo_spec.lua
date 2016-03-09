require('busted.runner')()

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
      local c = pomelo.newClient({enable_polling = false})
    end)
    it('construct a new client instance with options', function()
      local c = pomelo.newClient({
        conn_timeout = 30, -- optional, default 30 seconds
        enable_reconn = true, -- optional, default true
        reconn_max_retry = 3, -- optional, 'ALWAYS' or a positive integer. default 'ALWAYS'
        reconn_delay = 2, -- integer, optional, default to 2
        reconn_delay_max = 30, -- integer, optional, default to 30
        reconn_exp_backoff = true,-- boolean, optional, default to true
        enable_polling = true,
        transport_name = "TLS" -- 'TCP', 'TLS', 'DUMMY', or an integer id of you customized transport
      })
    end)
  end)
  describe('Client', function()
    local client
    after_each(function()
      client = nil
    end)

  end)
end)

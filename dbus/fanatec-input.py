#!/usr/bin/python

from pydbus.generic import signal
import glob
  
def get_sysfs_base(PID):
  return glob.glob("/sys/module/hid_ftec/drivers/hid:ftec_csl_elite/0003:0EB7:%s.*"%(PID))[0]

class CSLElite(object):
  """
    <node>
    </node>
  """
  def __init__(self):
    pass
  

class CSLElitePedals(object):
  """
    <node>
      <interface name='org.fanatec.CSLElite.Pedals'>
        <property name="Load" type="i" access="readwrite">
          <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
        </property>
      </interface>
    </node>
  """

  def __init__(self):
    pass
  
  def get_sysfs(self, name):
    return "%s/%s"%(get_sysfs_base('6204'), name)

  @property
  def Load(self):
    return int(open(self.get_sysfs('load'),'r').read())

  @Load.setter
  def Load(self, value):
    return int(open(self.get_sysfs('load'),'w').write(str(value)))

  PropertiesChanged = signal()


class CSLEliteWheel(object):
  """
    <node>
      <interface name='org.fanatec.CSLElite.Wheel'>
        <property name="Display" type="i" access="write">
          <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
        </property>
        <property name="RPM" type="ab" access="write">
          <annotation name="org.freedesktop.DBus.Property.EmitsChangedSignal" value="true"/>
        </property>        
      </interface>
    </node>
  """

  def __init__(self):
    pass

  def get_sysfs(self, name):
    return "%s/%s"%(get_sysfs_base('0005'), name)
  
  def get_sysfs_rpm(self):
    sysfs_base = get_sysfs_base('0005')
    return "%s/leds/0003:0EB7:0005.%s::RPM"%(sysfs_base,sysfs_base.split(".")[-1])

  @Display.setter
  def Display(self, value):
    return int(open(self.get_sysfs('display'),'w').write(str(value)))

  @RPM.setter
  def RPM(self, values):
    return list(map(lambda i: open('%s%i/brightness'%(self.get_sysfs_rpm(), i[0] + 1),'w').write('1' if i[1] else '0'), enumerate(values)))

  PropertiesChanged = signal()


if __name__ == "__main__":
  from pydbus import SystemBus
  from gi.repository import GLib

  bus = SystemBus()
  r = bus.publish('org.fanatec.CSLElite', CSLElite(),
    ('Pedals', CSLElitePedals()),
    ('Wheel', CSLEliteWheel()))
  try:
      loop = GLib.MainLoop()
      loop.run()
  except (KeyboardInterrupt, SystemExit):
      print("Exiting")
  except Exception as e:
      raise e        
  finally:
      r.unpublish()

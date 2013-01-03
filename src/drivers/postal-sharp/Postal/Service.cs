// 
// Copyright 2012 Catch.com
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

using System;
using System.Collections.Generic;
using System.Web.Script.Serialization;
using System.Net;
using System.Text;

namespace Postal
{
	public class Service
	{
		public string Host { get; set; }
		public ushort Port { get; set; }
		
		WebClient m_client;
		
		public Service (string host, ushort port)
		{
			this.Host = host;
			this.Port = port;
			this.m_client = new WebClient();
		}
		
		public Service () : this ("localhost", 5300)
		{
		}
		
		public Device AddDevice (Device device)
		{
			var url = String.Format ("http://{0}:{1}/v1/users/{2}/devices/{3}", Host, Port, device.User, device.DeviceToken);
			var serializer = new JavaScriptSerializer();
			var body = serializer.Serialize(device);
			var reply = m_client.UploadData(url, "PUT", Encoding.UTF8.GetBytes (body));
			var ret = serializer.Deserialize<Device>(Encoding.UTF8.GetString(reply));
			return ret;
		}
		
		public void RemoveDevice (Device device)
		{
			var url = String.Format ("http://{0}:{1}/v1/users/{2}/devices/{3}", Host, Port, device.User, device.DeviceToken);
			m_client.UploadData(url, "DELETE", null);
		}
		
		public Device GetDevice (string user, string device_token)
		{
			var url = String.Format ("http://{0}:{1}/v1/users/{2}/devices/{3}", Host, Port, user, device_token);
			var reply = m_client.DownloadString(url);
			var serializer = new JavaScriptSerializer();
			var ret = serializer.Deserialize<Device>(reply);
			return ret;
		}
		
		public List<Device> GetDevices (string user)
		{
			var url = String.Format ("http://{0}:{1}/v1/users/{2}/devices", Host, Port, user);
			var reply = m_client.DownloadString(url);
			var serializer = new JavaScriptSerializer();
			var ret = serializer.Deserialize<List<Device>>(reply);
			return ret;
		}
	}
}
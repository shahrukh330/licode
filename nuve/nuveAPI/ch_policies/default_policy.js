'use strict';
/*
Params

	room: room to which we need to assing a erizoController.
		{
		name: String,
		[p2p: bool],
		[data: Object],
		_id: ObjectId
		}

	ec_list: available erizo controllers
		{ erizoControllerId: {
        	ip: String,
        	rpcID: String,
        	state: Int,
        	keepAlive: Int,
        	hostname: String,
        	port: Int,
        	ssl: bool
   	 	}, ...}

   	ec_queue: array with erizo controllers priority according rooms load

Returns

	erizoControlerId: the key of the erizo controller selected from ec_list

*/
exports.getErizoController = function (room, ecList, ecQueue) {
  var erizoControllerId = ecQueue[0];
  return erizoControllerId;
};

/**
 *		Tempesta FW
 *
 * This file contains utils that help to test certain Tempesta FW modules.
 * They implement things like stubbing, mocking, generating data for testing.
 *
 * Actually things contained in this file are wrong a bit.
 * Good code tends to have most of the logic in pure stateless loosely-coupled
 * well-isolated functions that may be tested without faking any state.
 * But this is not reachable most of the time, especially when performance is
 * a goal and you have to build the architecture keeping it in mind.
 * So over time, we expect to see a decent amount of helpers here.
 *
 * These things are specific to Tempesta FW, so they are located here,
 * and generic testing functions/macros are located in test.c/test.h
 *
 * Copyright (C) 2014 NatSys Lab. (info@natsys-lab.com).
 * Copyright (C) 2015-2017 Tempesta Technologies, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "http_msg.h"

static TfwConn conn_req, conn_resp;

TfwHttpReq *
test_req_alloc(size_t data_len)
{
	int ret;
	TfwMsgIter it;
	TfwHttpMsg *hmreq;

	/* Actually there were more code here, mostly it was copy-paste from
	 * tfw_http_msg_alloc(). It is removed because we need to test how it
	 * initializes the message and we would not like to test the copy-paste.
	 */
	hmreq = __tfw_http_msg_alloc(Conn_HttpClnt, true);
	BUG_ON(!hmreq);

	ret = tfw_http_msg_setup(hmreq, &it, data_len);
	BUG_ON(ret);

	memset(&conn_req, 0, sizeof(TfwConn));
	tfw_connection_init(&conn_req);
	conn_req.proto.type = Conn_HttpClnt;
	hmreq->conn = &conn_req;

	return (TfwHttpReq *)hmreq;
}

void
test_req_free(TfwHttpReq *req)
{
	/* In tests we are stricter: we don't allow to free a NULL pointer
	 * to be sure exactly what we are free'ing and to catch bugs early. */
	BUG_ON(!req);

	tfw_http_msg_free((TfwHttpMsg *)req);
}

TfwHttpResp *
test_resp_alloc(size_t data_len)
{
	int ret;
	TfwMsgIter it;
	TfwHttpMsg *hmresp;

	hmresp = __tfw_http_msg_alloc(Conn_HttpSrv, true);
	BUG_ON(!hmresp);

	ret = tfw_http_msg_setup(hmresp, &it, data_len);
	BUG_ON(ret);

	memset(&conn_resp, 0, sizeof(TfwConn));
	tfw_connection_init(&conn_req);
	conn_resp.proto.type = Conn_HttpSrv;
	hmresp->conn = &conn_resp;

	return (TfwHttpResp *)hmresp;
}

void
test_resp_free(TfwHttpResp *resp)
{
	BUG_ON(!resp);
	tfw_http_msg_free((TfwHttpMsg *)resp);
}

int
test_parse_helper(TfwHttpMsg *hm, ss_skb_actor_t actor)
{
	struct sk_buff *skb;
	unsigned int off;

	skb = hm->msg.head_skb;
	BUG_ON(!skb);
	off = 0;
	while (1) {
		hm->parser.skb = skb;
		switch (ss_skb_process(skb, &off, ULONG_MAX, actor, hm)) {
		case TFW_POSTPONE:
			if (skb->next == hm->msg.head_skb)
				return -1;
			skb = skb->next;
			continue;

		case TFW_PASS:
			/* successfully parsed */
			return 0;

		default:
			return -1;
		}
	}
}

int test_parse_str_helper(TfwHttpMsg *hm, ss_skb_actor_t actor,
			  unsigned char *str, size_t len, size_t chunks)
{
	size_t chlen = len / chunks, rem = len % chunks, pos = 0, step;
	int r = 0;

	/* Data comes not from skb, so just use the very first skb. */
	hm->parser.skb = hm->msg.head_skb;
	BUG_ON(!hm->parser.skb);

	while (pos < len) {
		step = chlen;
		if (rem) {
			step += rem;
			rem = 0;
		}

		r = actor(hm, str + pos, step);
		if (r != TFW_POSTPONE)
			return r;
		pos += step;
	}

	return r;
}
